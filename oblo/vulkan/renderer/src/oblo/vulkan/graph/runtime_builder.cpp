#include <oblo/vulkan/graph/runtime_builder.hpp>

#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>

namespace oblo::vk
{
    namespace
    {
        VkImageUsageFlags convert_usage(resource_usage usage)
        {
            switch (usage)
            {
            case resource_usage::depth_stencil_read:
            case resource_usage::depth_stencil_write:
                return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            case resource_usage::render_target_write:
                return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            case resource_usage::shader_read:
                return VK_IMAGE_USAGE_SAMPLED_BIT;

            default:
                OBLO_ASSERT(false);
                return {};
            };
        }

        VkImageLayout convert_layout(resource_usage usage)
        {
            switch (usage)
            {
            case resource_usage::depth_stencil_read:
            case resource_usage::depth_stencil_write:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            case resource_usage::render_target_write:
                return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

            case resource_usage::shader_read:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            case resource_usage::transfer_destination:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

            case resource_usage::transfer_source:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            default:
                OBLO_ASSERT(false);
                return {};
            };
        }
    }

    void runtime_builder::create(
        resource<texture> texture, const texture2d_initializer& initializer, resource_usage usage)
    {
        const image_initializer imageInitializer{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = initializer.format,
            .extent = {.width = initializer.width, .height = initializer.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = convert_usage(usage),
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .memoryUsage = memory_usage::gpu_only,
        };

        // TODO
        constexpr lifetime_range range{0, 0};

        const auto poolIndex = m_resourcePool->add(imageInitializer, range);
        m_graph->add_transient_resource(texture, poolIndex);
        m_graph->add_resource_transition(texture, convert_layout(usage));
    }

    void runtime_builder::acquire(resource<texture> texture, resource_usage usage)
    {
        m_graph->add_resource_transition(texture, convert_layout(usage));

        switch (usage)
        {
        case resource_usage::transfer_destination:
            m_resourcePool->add_usage(m_graph->find_pool_index(texture), VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            break;

        case resource_usage::transfer_source:
            m_resourcePool->add_usage(m_graph->find_pool_index(texture), VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            break;

        default:
            break;
        }
    }
}