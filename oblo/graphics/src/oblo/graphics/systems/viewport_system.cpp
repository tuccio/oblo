#include <oblo/graphics/systems/viewport_system.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/vulkan/create_render_target.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/topology_builder.hpp>
#include <oblo/vulkan/nodes/debug_triangle_node.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/renderer_context.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
#include <oblo/vulkan/texture.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::graphics
{
    namespace
    {
        constexpr std::string_view OutFinalRenderTarget{"Final Render Target"};
        constexpr std::string_view InResolution{"Resolution"};
    }

    struct viewport_system::render_graph_data
    {
        bool isAlive{};
        h32<vk::render_graph> id{};
        h32<vk::texture> texture{};
        u32 width{};
        u32 height{};
        VkDescriptorSet descriptorSet{};
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
        }

        if (renderGraphData.descriptorSet)
        {
            vkCtx.destroy_deferred(renderGraphData.descriptorSet, m_descriptorPool, submitIndex);
            renderGraphData.descriptorSet = {};
        }
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

        for (auto& renderGraphData : m_renderGraphs.values())
        {
            // Set to false to garbage collect
            renderGraphData.isAlive = false;
        }

        for (const auto [entities, viewports] : ctx.entities->range<viewport_component>())
        {
            for (auto&& [entity, viewport] : zip_range(entities, viewports))
            {
                auto* renderGraphData = m_renderGraphs.try_find(entity);
                render_graph* graph;

                if (!renderGraphData)
                {
                    expected res = topology_builder{}
                                       .add_node<debug_triangle_node>()
                                       .add_output<h32<texture>>(OutFinalRenderTarget)
                                       .add_input<vec2u>(InResolution)
                                       .connect_output(&debug_triangle_node::outRenderTarget, OutFinalRenderTarget)
                                       .connect_input(InResolution, &debug_triangle_node::inResolution)
                                       .build();

                    if (!res)
                    {
                        continue;
                    }

                    const auto [it, ok] = m_renderGraphs.emplace(entity);
                    it->isAlive = true;
                    it->id = m_renderer->add(std::move(*res));

                    renderGraphData = &*it;

                    graph = m_renderer->find(it->id);
                }
                else
                {
                    renderGraphData->isAlive = true;
                    graph = m_renderer->find(renderGraphData->id);
                }

                constexpr auto viewportImageLayout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

                if (!renderGraphData->texture || renderGraphData->width != viewport.width ||
                    renderGraphData->height != viewport.height)
                {
                    // TODO: If descriptor set already exists, destroy
                    // TODO: If texture already exists, unregister and dstroy

                    destroy_graph_vulkan_objects(*renderGraphData);

                    const auto result = vk::create_2d_render_target(m_renderer->get_allocator(),
                        viewport.width,
                        viewport.height,
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

                    renderGraphData->width = viewport.width;
                    renderGraphData->height = viewport.height;
                    renderGraphData->texture = rm.register_texture(*result, VK_IMAGE_LAYOUT_UNDEFINED);

                    viewport.imageId = renderGraphData->descriptorSet;
                }

                if (auto* const resolution = graph->find_input<vec2u>(InResolution))
                {
                    *resolution = vec2u{viewport.width, viewport.height};
                }

                graph->copy_output(OutFinalRenderTarget, renderGraphData->texture, viewportImageLayout);
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

                m_renderer->remove(renderGraphData.id);
                elementsToRemove[numRemovedElements] = entity;
                ++numRemovedElements;
            }

            for (auto e : std::span(elementsToRemove, numRemovedElements))
            {
                m_renderGraphs.erase(e);
            }
        }

        // TODO: Find a better home for the renderer update
        m_renderer->update();
    }
}