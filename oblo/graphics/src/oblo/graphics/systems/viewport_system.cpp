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
#include <oblo/graphics/systems/scene_renderer.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/math/view_projection.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/data/camera_buffer.hpp>
#include <oblo/vulkan/data/copy_texture_info.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/data/time_buffer.hpp>
#include <oblo/vulkan/draw/binding_table.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/templates/graph_templates.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo
{
    namespace
    {
        // Actually a u32, but the buffer requires alignment
        constexpr u32 PickingResultSize{16};
    }

    struct viewport_system::render_graph_data
    {
        bool isAlive{};
        h32<vk::frame_graph_subgraph> subgraph{};
        h32<vk::texture> texture{};
        u32 width{};
        u32 height{};
        VkDescriptorSet descriptorSet{};
        VkImage image{};

        vk::allocated_buffer pickingOutputBuffer;
        u64 lastPickingSubmitIndex{};
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
            m_renderer->get_frame_graph().remove(graphData.subgraph);
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

        if (renderGraphData.pickingOutputBuffer.buffer)
        {
            vkCtx.destroy_deferred(renderGraphData.pickingOutputBuffer.buffer, submitIndex);
            vkCtx.destroy_deferred(renderGraphData.pickingOutputBuffer.allocation, submitIndex);
            renderGraphData.pickingOutputBuffer = {};
        }
    }

    bool viewport_system::prepare_picking_buffers(render_graph_data& graphData)
    {
        auto& allocator = m_renderer->get_allocator();

        if (!graphData.pickingOutputBuffer.buffer &&
            allocator.create_buffer(
                {
                    .size = PickingResultSize,
                    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    .memoryUsage = vk::memory_usage::gpu_to_cpu,
                },
                &graphData.pickingOutputBuffer) != VK_SUCCESS)
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

        m_sceneRenderer = ctx.services->find<scene_renderer>();
        OBLO_ASSERT(m_sceneRenderer);
        m_sceneRenderer->ensure_setup();

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

                if (!renderGraphData)
                {
                    const auto [it, ok] = m_renderGraphs.emplace(entity);
                    it->isAlive = true;

                    const auto registry = vk::create_frame_graph_registry();

                    const auto mainViewTemplate = vk::main_view::create(registry,
                        {
                            .withPicking = true,
                        });

                    const auto subgraph = frameGraph.instantiate(mainViewTemplate);
                    it->subgraph = subgraph;

                    m_sceneRenderer->add_scene_view(subgraph);

                    renderGraphData = &*it;
                }
                else
                {
                    renderGraphData->isAlive = true;
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
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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

                frameGraph
                    .set_input(renderGraphData->subgraph, main_view::InResolution, vec2u{renderWidth, renderHeight})
                    .assert_value();

                frameGraph
                    .set_input(renderGraphData->subgraph,
                        main_view::InTime,
                        time_buffer{
                            .frameIndex = m_frameIndex,
                        })
                    .assert_value();

                frameGraph
                    .set_input(renderGraphData->subgraph,
                        main_view::InFinalRenderTarget,
                        copy_texture_info{
                            .image = renderGraphData->image,
                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            //.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                        })
                    .assert_value();

                {
                    // TODO: Deal with errors, also transposing would be enough here most likely
                    const mat4 view = *inverse(transform.localToWorld);

                    const f32 ratio = f32(viewport.height) / viewport.width;
                    const mat4 proj = make_perspective_matrix(camera.fovy, ratio, camera.near, camera.far);

                    const mat4 viewProj = proj * view;
                    const mat4 invViewProj = inverse(viewProj).assert_value_or(mat4::identity());
                    const mat4 invProj = inverse(proj).assert_value_or(mat4::identity());

                    const vec4 position = transform.localToWorld.columns[3];

                    const camera_buffer cameraBuffer{
                        .view = view,
                        .projection = proj,
                        .viewProjection = viewProj,
                        .invViewProjection = invViewProj,
                        .invProjection = invProj,
                        .frustum = make_frustum_from_inverse_view_projection(invViewProj),
                        .position = {position.x, position.y, position.z},
                    };

                    frameGraph.set_input(renderGraphData->subgraph, main_view::InCamera, cameraBuffer).assert_value();
                }

                {
                    picking_configuration pickingConfig{};

                    switch (viewport.picking.state)
                    {
                    case picking_request::state::requested:
                        if (prepare_picking_buffers(*renderGraphData))
                        {
                            pickingConfig = {
                                .enabled = true,
                                .coordinates = viewport.picking.coordinates,
                                .outputBuffer =
                                    {
                                        .buffer = renderGraphData->pickingOutputBuffer.buffer,
                                        .size = PickingResultSize,
                                    },
                            };

                            viewport.picking.state = picking_request::state::awaiting;
                        }
                        else
                        {
                            viewport.picking.state = picking_request::state::failed;
                        }

                        break;

                    case picking_request::state::awaiting:

                        if (m_renderer->get_vulkan_context().is_submit_done(renderGraphData->lastPickingSubmitIndex))
                        {
                            auto& allocator = m_renderer->get_allocator();

                            if (void* ptr;
                                allocator.map(renderGraphData->pickingOutputBuffer.allocation, &ptr) == VK_SUCCESS)
                            {
                                std::memcpy(&viewport.picking.result, ptr, sizeof(u32));
                                viewport.picking.state = picking_request::state::served;

                                allocator.unmap(renderGraphData->pickingOutputBuffer.allocation);
                            }
                            else
                            {
                                viewport.picking.state = picking_request::state::failed;
                            }
                        }

                        break;

                    default:
                        break;
                    }

                    frameGraph.set_input(renderGraphData->subgraph, main_view::InPickingConfiguration, pickingConfig)
                        .assert_value();
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

                m_renderer->get_frame_graph().remove(renderGraphData.subgraph);
                m_sceneRenderer->remove_scene_view(renderGraphData.subgraph);

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