#include <oblo/vulkan/graph/runtime_builder.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>
#include <oblo/vulkan/graph/resource_pool.hpp>
#include <oblo/vulkan/renderer.hpp>
#include <oblo/vulkan/staging_buffer.hpp>

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
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

        VkBufferUsageFlags convert_buffer_usage(buffer_usage usage)
        {
            VkBufferUsageFlags result{};

            OBLO_ASSERT(usage != buffer_usage::enum_max);

            switch (usage)
            {
            case buffer_usage::storage_read:
            case buffer_usage::storage_write:
                result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                break;

            case buffer_usage::indirect:
                result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
                break;

            case buffer_usage::uniform:
                result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;

                // case buffer_usage::transfer_source:
                //     result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                //     break;

                // case buffer_usage::transfer_destination:
                //     result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                //     break;

            default:
                unreachable();
            }

            return result;
        }

        std::pair<VkPipelineStageFlags2, VkAccessFlags2> convert_for_sync2(pass_kind passKind, buffer_usage usage)
        {
            VkPipelineStageFlags2 pipelineStage{};
            VkAccessFlags2 access{};

            switch (passKind)
            {
            case pass_kind::graphics:
                pipelineStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                break;

            case pass_kind::compute:
                pipelineStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                break;
            }

            switch (usage)
            {
            case buffer_usage::storage_read:
                access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
                break;
            case buffer_usage::storage_write:
                access = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
                break;
            case buffer_usage::uniform:
                access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT;
                break;
            case buffer_usage::indirect:
                access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
                break;
            default:
                unreachable();
            }

            return {pipelineStage, access};
        }
    }

    void runtime_builder::create(
        resource<texture> texture, const transient_texture_initializer& initializer, resource_usage usage) const
    {
        const image_initializer imageInitializer{
            .imageType = VK_IMAGE_TYPE_2D,
            .format = initializer.format,
            .extent = {.width = initializer.width, .height = initializer.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = convert_usage(usage) | VK_IMAGE_USAGE_SAMPLED_BIT | initializer.usage,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .memoryUsage = memory_usage::gpu_only,
        };

        // TODO: (#29) Reuse and alias texture memory
        constexpr lifetime_range range{0, 0};

        const auto poolIndex = m_resourcePool->add(imageInitializer, range);
        m_graph->add_transient_resource(texture, poolIndex);
        m_graph->add_resource_transition(texture, convert_layout(usage));
    }

    void runtime_builder::create(resource<buffer> buffer,
        const transient_buffer_initializer& initializer,
        pass_kind passKind,
        buffer_usage usage) const
    {
        auto vkUsage = convert_buffer_usage(usage);

        if (!initializer.data.empty())
        {
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        const auto poolIndex = m_resourcePool->add_buffer(initializer.size, vkUsage);

        staging_buffer_span stagedData{};
        staging_buffer_span* stagedDataPtr{};

        if (!initializer.data.empty())
        {
            [[maybe_unused]] const auto res = m_renderer->get_staging_buffer().stage(initializer.data);
            OBLO_ASSERT(res, "Out of space on the staging buffer, we should flush instead");

            stagedData = *res;
            stagedDataPtr = &stagedData;

            // All copies will be added to the command buffer upfront, so we start with a buffer that has been
            // transfered to
            m_graph->add_buffer_access(buffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        }

        m_graph->add_transient_buffer(buffer, poolIndex, stagedDataPtr);

        const auto [pipelineStage, access] = convert_for_sync2(passKind, usage);
        m_graph->add_buffer_access(buffer, pipelineStage, access);
    }

    void runtime_builder::create(
        resource<buffer> buffer, const staging_buffer_span& stagedData, pass_kind passKind, buffer_usage usage) const
    {
        auto vkUsage = convert_buffer_usage(usage);

        const auto stagedDataSize = calculate_size(stagedData);

        if (stagedDataSize != 0)
        {
            vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        const auto poolIndex = m_resourcePool->add_buffer(stagedDataSize, vkUsage);

        // All copies will be added to the command buffer upfront, so we start with a buffer that has been
        // transfered to
        m_graph->add_buffer_access(buffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        m_graph->add_transient_buffer(buffer, poolIndex, &stagedData);

        const auto [pipelineStage, access] = convert_for_sync2(passKind, usage);
        m_graph->add_buffer_access(buffer, pipelineStage, access);
    }

    void runtime_builder::acquire(resource<texture> texture, resource_usage usage) const
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

    void runtime_builder::acquire(resource<buffer> buffer, pass_kind passKind, buffer_usage usage) const
    {
        const auto poolIndex = m_graph->find_pool_index(buffer);
        m_resourcePool->add_buffer_usage(poolIndex, convert_buffer_usage(usage));
        const auto [pipelineStage, access] = convert_for_sync2(passKind, usage);
        m_graph->add_buffer_access(buffer, pipelineStage, access);
    }

    resource<buffer> runtime_builder::create_dynamic_buffer(
        const transient_buffer_initializer& initializer, pass_kind passKind, buffer_usage usage) const
    {
        const auto pinHandle = m_graph->allocate_dynamic_resource_pin();

        const resource<buffer> resource{pinHandle};
        create(resource, initializer, passKind, usage);

        return resource;
    }

    resource<buffer> runtime_builder::create_dynamic_buffer(
        const staging_buffer_span& stagedData, pass_kind passKind, buffer_usage usage) const
    {
        const auto pinHandle = m_graph->allocate_dynamic_resource_pin();

        const resource<buffer> resource{pinHandle};
        create(resource, stagedData, passKind, usage);

        return resource;
    }

    frame_allocator& runtime_builder::get_frame_allocator() const
    {
        return *m_graph->m_dynamicAllocator;
    }

    const draw_registry& runtime_builder::get_draw_registry() const
    {
        return m_renderer->get_draw_registry();
    }

    void* runtime_builder::access_resource_storage(u32 index) const
    {
        return m_graph->access_resource_storage(index);
    }
}