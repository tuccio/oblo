#include <oblo/graphics/systems/viewport_system.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/frame_allocator.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/math/view_projection.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/draw/buffer_binding_table.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/topology_builder.hpp>
#include <oblo/vulkan/nodes/bypass_culling.hpp>
#include <oblo/vulkan/nodes/copy_texture_node.hpp>
#include <oblo/vulkan/nodes/debug_draw_all.hpp>
#include <oblo/vulkan/nodes/debug_triangle_node.hpp>
#include <oblo/vulkan/nodes/forward_pass.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/nodes/picking_readback.hpp>
#include <oblo/vulkan/nodes/view_buffers_node.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    namespace
    {
        constexpr std::string_view InPickingConfiguration{"Picking Configuration"};

        constexpr std::string_view OutFinalRenderTarget{"Final Render Target"};
        constexpr std::string_view OutPickingBuffer{"Picking Buffer"};
        constexpr std::string_view InResolution{"Resolution"};
        constexpr std::string_view InCamera{"Camera"};
        constexpr std::string_view InTime{"Time"};
        constexpr std::string_view InFinalRenderTarget{"Final Render Target"};

        constexpr u32 PickingResultSize{sizeof(u32)};

        constexpr bool UseNewFrameGraph{false};

        vk::frame_graph_template create_main_view(const vk::frame_graph_registry& registry)
        {
            using namespace oblo::vk;

            vk::frame_graph_template graph;

            graph.init(registry);

            const auto viewBuffers = graph.add_node<view_buffers_node>();
            const auto frustumCulling = graph.add_node<frustum_culling>();
            const auto forwardPass = graph.add_node<forward_pass>();
            const auto pickingReadback = graph.add_node<picking_readback>();
            const auto copyFinalTarget = graph.add_node<copy_texture_node>();

            graph.make_input(viewBuffers, &view_buffers_node::inCameraData, InCamera);
            graph.make_input(viewBuffers, &view_buffers_node::inTimeData, InTime);

            graph.make_input(forwardPass, &forward_pass::inResolution, InResolution);
            graph.make_input(forwardPass, &forward_pass::inPickingConfiguration, InPickingConfiguration);

            graph.make_input(copyFinalTarget, &copy_texture_node::inTarget, InFinalRenderTarget);

            graph.connect(viewBuffers,
                &view_buffers_node::outPerViewBindingTable,
                forwardPass,
                &forward_pass::inPerViewBindingTable);

            graph.connect(viewBuffers,
                &view_buffers_node::outPerViewBindingTable,
                frustumCulling,
                &frustum_culling::inPerViewBindingTable);

            graph.connect(forwardPass,
                &forward_pass::outPickingIdBuffer,
                pickingReadback,
                &picking_readback::inPickingIdBuffer);

            graph.connect(frustumCulling, &frustum_culling::outDrawBufferData, forwardPass, &forward_pass::inDrawData);

            graph.connect(forwardPass, &forward_pass::outRenderTarget, copyFinalTarget, &copy_texture_node::inSource);

            return graph;
        }

        vk::frame_graph_registry create_frame_graph_registry()
        {
            using namespace vk;
            frame_graph_registry registry;

            registry.register_node<copy_texture_node>();
            registry.register_node<view_buffers_node>();
            registry.register_node<frustum_culling>();
            registry.register_node<forward_pass>();
            registry.register_node<picking_readback>();

            return registry;
        }
    }

    struct viewport_system::render_graph_data
    {
        bool isAlive{};
        h32<vk::render_graph> id{};
        h32<vk::texture> texture{};
        u32 width{};
        u32 height{};
        VkDescriptorSet descriptorSet{};
        VkImage image{};

        vk::allocated_buffer pickingDownloadBuffer;
        u64 lastPickingSubmitIndex{};

        h32<vk::frame_graph_subgraph> subgraph{};
    };

    viewport_system::viewport_system() = default;

    viewport_system::~viewport_system()
    {
        if (!m_renderer)
        {
            return;
        }

        for (auto& graphData : m_renderGraphs.values())
        {
            m_renderer->remove(graphData.id);
            destroy_graph_vulkan_objects(graphData);
        }

        auto& vkCtx = m_renderer->get_vulkan_context();

        const auto submitIndex = vkCtx.get_submit_index();

        if (m_sampler)
        {
            vkCtx.destroy_deferred(m_sampler, submitIndex);
        }

        if (m_descriptorSetLayout)
        {
            vkCtx.destroy_deferred(m_descriptorSetLayout, submitIndex);
        }

        if (m_descriptorPool)
        {
            vkCtx.destroy_deferred(m_descriptorPool, submitIndex);
        }
    }

    void viewport_system::create_vulkan_objects()
    {
        const auto* const allocationCbs = m_renderer->get_allocator().get_allocation_callbacks();
        const VkDevice device = m_renderer->get_engine().get_device();

        {
            constexpr auto MaxSets{64};
            constexpr VkDescriptorPoolSize descriptorSizes[] = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxSets},
            };

            const VkDescriptorPoolCreateInfo poolCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = MaxSets,
                .poolSizeCount = array_size(descriptorSizes),
                .pPoolSizes = descriptorSizes,
            };

            OBLO_VK_PANIC(vkCreateDescriptorPool(device, &poolCreateInfo, allocationCbs, &m_descriptorPool));
        }

        {
            const VkDescriptorSetLayoutBinding binding[] = {{
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            }};

            const VkDescriptorSetLayoutCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = binding,
            };

            OBLO_VK_PANIC(vkCreateDescriptorSetLayout(device, &info, allocationCbs, &m_descriptorSetLayout));
        }

        {
            constexpr VkSamplerCreateInfo samplerInfo{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .anisotropyEnable = VK_FALSE,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            };

            OBLO_VK_PANIC(vkCreateSampler(device, &samplerInfo, allocationCbs, &m_sampler));
        }
    }

    void viewport_system::destroy_graph_vulkan_objects(render_graph_data& renderGraphData)
    {
        auto& vkCtx = m_renderer->get_vulkan_context();
        const auto submitIndex = vkCtx.get_submit_index();

        if (renderGraphData.texture)
        {
            vkCtx.destroy_deferred(renderGraphData.texture, submitIndex);
            renderGraphData.texture = {};
            renderGraphData.image = {};
        }

        if (renderGraphData.descriptorSet)
        {
            vkCtx.destroy_deferred(renderGraphData.descriptorSet, m_descriptorPool, submitIndex);
            renderGraphData.descriptorSet = {};
        }

        if (renderGraphData.pickingDownloadBuffer.buffer)
        {
            vkCtx.destroy_deferred(renderGraphData.pickingDownloadBuffer.buffer, submitIndex);
            vkCtx.destroy_deferred(renderGraphData.pickingDownloadBuffer.allocation, submitIndex);
            renderGraphData.pickingDownloadBuffer = {};
        }
    }

    bool viewport_system::prepare_picking_buffers(render_graph_data& graphData)
    {
        auto& allocator = m_renderer->get_allocator();

        if (!graphData.pickingDownloadBuffer.buffer &&
            allocator.create_buffer(
                {
                    .size = PickingResultSize,
                    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memoryUsage = vk::memory_usage::gpu_to_cpu,
                },
                &graphData.pickingDownloadBuffer) != VK_SUCCESS)
        {
            return false;
        }

        graphData.lastPickingSubmitIndex = m_renderer->get_vulkan_context().get_submit_index();

        return true;
    }

    void viewport_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<vk::renderer>();
        OBLO_ASSERT(m_renderer);

        create_vulkan_objects();

        update(ctx);
    }

    void viewport_system::update(const ecs::system_update_context& ctx)
    {
        using namespace oblo::vk;

        const auto device = m_renderer->get_engine().get_device();

        auto& rm = m_renderer->get_resource_manager();
        auto& frameGraph = m_renderer->get_frame_graph();

        for (auto& renderGraphData : m_renderGraphs.values())
        {
            // Set to false to garbage collect
            renderGraphData.isAlive = false;
        }

        for (const auto [entities, transforms, cameras, viewports] :
            ctx.entities->range<global_transform_component, camera_component, viewport_component>())
        {
            for (auto&& [entity, transform, camera, viewport] : zip_range(entities, transforms, cameras, viewports))
            {
                auto* renderGraphData = m_renderGraphs.try_find(entity);
                render_graph* graph;

                if (!renderGraphData)
                {
#if 1
                    topology_builder graphBuilder;

                    graphBuilder.add_node<forward_pass>()
                        .add_node<view_buffers_node>()
                        .add_node<picking_readback>()
                        .add_output<h32<vk::texture>>(OutFinalRenderTarget)
                        .add_output<h32<vk::texture>>(OutPickingBuffer)
                        .add_input<vec2u>(InResolution)
                        .add_input<camera_buffer>(InCamera)
                        .add_input<time_buffer>(InTime)
                        .add_input<picking_configuration>(InPickingConfiguration)
                        .connect_output(&forward_pass::outRenderTarget, OutFinalRenderTarget)
                        .connect_input(InCamera, &view_buffers_node::inCameraData)
                        .connect_input(InTime, &view_buffers_node::inTimeData)
                        .connect_input(InResolution, &forward_pass::inResolution)
                        .connect_input(InPickingConfiguration, &forward_pass::inPickingConfiguration)
                        .connect_input(InPickingConfiguration, &picking_readback::inPickingConfiguration)
                        .connect(&view_buffers_node::outPerViewBindingTable, &forward_pass::inPerViewBindingTable)
                        .connect(&forward_pass::outPickingIdBuffer, &picking_readback::inPickingIdBuffer);

                    constexpr bool withFrustumCulling{true};

                    if constexpr (withFrustumCulling)
                    {
                        graphBuilder.add_node<frustum_culling>()
                            .connect(&view_buffers_node::outPerViewBindingTable,
                                &frustum_culling::inPerViewBindingTable)
                            .connect(&frustum_culling::outDrawBufferData, &forward_pass::inDrawData);
                    }
                    else
                    {
                        graphBuilder.add_node<bypass_culling>().connect(&bypass_culling::outDrawBufferData,
                            &forward_pass::inDrawData);
                    }

                    expected res = graphBuilder.build();
#else
                    expected res =
                        topology_builder{}
                            .add_node<debug_draw_all>()
                            .add_node<view_buffers_node>()
                            .add_output<h32<vk::texture>>(OutFinalRenderTarget)
                            .add_input<vec2u>(InResolution)
                            .add_input<camera_buffer>(InCamera)
                            .connect_output(&debug_draw_all::outRenderTarget, OutFinalRenderTarget)
                            .connect_input(InCamera, &view_buffers_node::inCameraData)
                            .connect_input(InResolution, &debug_draw_all::inResolution)
                            .connect(&view_buffers_node::outPerViewBindingTable, &debug_draw_all::inPerViewBindingTable)
                            .build();
#endif

                    if (!res)
                    {
                        continue;
                    }

                    const auto [it, ok] = m_renderGraphs.emplace(entity);
                    it->isAlive = true;

                    if (UseNewFrameGraph)
                    {
                        const auto registry = create_frame_graph_registry();
                        const auto mainViewTemplate = create_main_view(registry);

                        const auto subgraph = frameGraph.instantiate(mainViewTemplate);
                        it->subgraph = subgraph;
                    }
                    else
                    {
                        it->id = m_renderer->add(std::move(*res));
                    }

                    renderGraphData = &*it;

                    graph = m_renderer->find(it->id);
                }
                else
                {
                    renderGraphData->isAlive = true;
                    graph = m_renderer->find(renderGraphData->id);
                }

                constexpr auto viewportImageLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

                const u32 renderWidth = max(viewport.width, 1u);
                const u32 renderHeight = max(viewport.height, 1u);

                if (!renderGraphData->texture || renderGraphData->width != renderWidth ||
                    renderGraphData->height != renderHeight)
                {
                    // TODO: If descriptor set already exists, destroy
                    // TODO: If texture already exists, unregister and destroy

                    destroy_graph_vulkan_objects(*renderGraphData);

                    const auto result = vk::create_2d_render_target(m_renderer->get_allocator(),
                        renderWidth,
                        renderHeight,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT);

                    OBLO_ASSERT(result);

                    const VkDescriptorSetAllocateInfo allocInfo{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .descriptorPool = m_descriptorPool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &m_descriptorSetLayout,
                    };

                    OBLO_VK_PANIC(vkAllocateDescriptorSets(device, &allocInfo, &renderGraphData->descriptorSet));

                    const VkDescriptorImageInfo descImage[] = {
                        {
                            .sampler = m_sampler,
                            .imageView = result->view,
                            .imageLayout = viewportImageLayout,
                        },
                    };

                    const VkWriteDescriptorSet writeDesc[] = {{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = renderGraphData->descriptorSet,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = descImage,
                    }};

                    vkUpdateDescriptorSets(device, 1, writeDesc, 0, nullptr);

                    renderGraphData->width = renderWidth;
                    renderGraphData->height = renderHeight;
                    renderGraphData->texture = rm.register_texture(*result, VK_IMAGE_LAYOUT_UNDEFINED);
                    renderGraphData->image = result->image;

                    viewport.imageId = renderGraphData->descriptorSet;
                }

                if constexpr (!UseNewFrameGraph)
                {
                    if (auto* const resolution = graph->find_input<vec2u>(InResolution))
                    {
                        *resolution = vec2u{renderWidth, renderHeight};
                    }

                    if (auto* const cameraBuffer = graph->find_input<camera_buffer>(InCamera))
                    {
                        // TODO: Deal with errors, also transposing would be enough here most likely
                        const mat4 view = *inverse(transform.value);

                        const f32 ratio = f32(viewport.height) / viewport.width;
                        const auto proj = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

                        const mat4 invViewProj = *inverse(proj * view);

                        *cameraBuffer = camera_buffer{
                            .view = view,
                            .projection = proj,
                            .viewProjection = proj * view,
                            .frustum = make_frustum_from_inverse_view_projection(invViewProj),
                        };
                    }

                    if (auto* const timeBuffer = graph->find_input<time_buffer>(InTime))
                    {
                        *timeBuffer = {
                            .frameIndex = m_frameIndex,
                        };
                    }

                    if (auto* const pickingCfg = graph->find_input<picking_configuration>(InPickingConfiguration))
                    {
                        switch (viewport.picking.state)
                        {
                        case picking_request::state::requested:
                            if (prepare_picking_buffers(*renderGraphData))
                            {
                                pickingCfg->enabled = true;
                                pickingCfg->coordinates = viewport.picking.coordinates;

                                pickingCfg->downloadBuffer = {
                                    .buffer = renderGraphData->pickingDownloadBuffer.buffer,
                                    .size = PickingResultSize,
                                };

                                viewport.picking.state = picking_request::state::awaiting;
                            }
                            else
                            {
                                viewport.picking.state = picking_request::state::failed;
                            }

                            break;

                        case picking_request::state::awaiting:

                            if (m_renderer->get_vulkan_context().is_submit_done(
                                    renderGraphData->lastPickingSubmitIndex))
                            {
                                auto& allocator = m_renderer->get_allocator();

                                if (void* ptr; allocator.map(renderGraphData->pickingDownloadBuffer.allocation, &ptr) ==
                                    VK_SUCCESS)
                                {
                                    std::memcpy(&viewport.picking.result, ptr, sizeof(u32));
                                    viewport.picking.state = picking_request::state::served;

                                    allocator.unmap(renderGraphData->pickingDownloadBuffer.allocation);
                                }
                                else
                                {
                                    viewport.picking.state = picking_request::state::failed;
                                }
                            }

                            pickingCfg->enabled = false;
                            break;

                        default:
                            pickingCfg->enabled = false;
                            break;
                        }
                    }

                    if (renderGraphData->texture)
                    {
                        graph->copy_output(OutFinalRenderTarget, renderGraphData->texture, viewportImageLayout);
                    }
                }
                else
                {
                    frameGraph.set_input(renderGraphData->subgraph, InResolution, vec2u{renderWidth, renderHeight})
                        .or_panic();

                    frameGraph
                        .set_input(renderGraphData->subgraph,
                            InTime,
                            time_buffer{
                                .frameIndex = m_frameIndex,
                            })
                        .or_panic();

                    frameGraph
                        .set_input(renderGraphData->subgraph,
                            InFinalRenderTarget,
                            copy_texture_info{
                                .image = renderGraphData->image,
                                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                            })
                        .or_panic();

                    {
                        // TODO: Deal with errors, also transposing would be enough here most likely
                        const mat4 view = *inverse(transform.value);

                        const f32 ratio = f32(viewport.height) / viewport.width;
                        const mat4 proj = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

                        const mat4 invViewProj = *inverse(proj * view);

                        const camera_buffer cameraBuffer{
                            .view = view,
                            .projection = proj,
                            .viewProjection = proj * view,
                            .frustum = make_frustum_from_inverse_view_projection(invViewProj),
                        };

                        frameGraph.set_input(renderGraphData->subgraph, InCamera, cameraBuffer).or_panic();
                    }

                    // TODO: Picking
                }
            }
        }

        if (!m_renderGraphs.empty())
        {
            // TODO: Should implement an iterator for flat_dense_map instead, and erase using that
            auto* const elementsToRemove = allocate_n<ecs::entity>(*ctx.frameAllocator, m_renderGraphs.size());
            u32 numRemovedElements{0};

            for (auto&& [entity, renderGraphData] : zip_range(m_renderGraphs.keys(), m_renderGraphs.values()))
            {
                if (renderGraphData.isAlive)
                {
                    continue;
                }

                if constexpr (UseNewFrameGraph)
                {
                    // TODO: Implement subgraph removal
                    OBLO_ASSERT(false);
                }
                else
                {
                    m_renderer->remove(renderGraphData.id);
                }

                elementsToRemove[numRemovedElements] = entity;
                ++numRemovedElements;
            }

            for (auto e : std::span(elementsToRemove, numRemovedElements))
            {
                m_renderGraphs.erase(e);
            }
        }

        ++m_frameIndex;
    }
}