#include <oblo/vulkan/graph/image_layout_tracker.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/draw/types.hpp>
#include <oblo/vulkan/graph/frame_graph_context.hpp>
#include <oblo/vulkan/graph/types_internal.hpp>
#include <oblo/vulkan/texture.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr bool is_depth_format(VkFormat format)
        {
            return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
                format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_S8_UINT ||
                format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        constexpr bool has_stencil(VkFormat format)
        {
            return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        VkPipelineStageFlags2 deduce_stage_mask(pass_kind newPass)
        {
            switch (newPass)
            {
            case pass_kind::compute:
                return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            case pass_kind::graphics:
                // We could actually check in which stage a texture is read using reflection
                return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            case pass_kind::raytracing:
                return VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            default:
                return 0;
            }
        }

        void deduce_barrier(pass_kind newPass, texture_usage newUsage, VkImageMemoryBarrier2& outBarrier)
        {
            switch (newUsage)
            {
            case texture_usage::depth_stencil_read:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                outBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                outBarrier.dstStageMask =
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case texture_usage::depth_stencil_write:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                outBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                outBarrier.dstStageMask =
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case texture_usage::render_target_write:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                outBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                outBarrier.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;

            case texture_usage::shader_read:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                outBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                outBarrier.dstStageMask = deduce_stage_mask(newPass);
                break;

            case texture_usage::storage_read:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                outBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                outBarrier.dstStageMask = deduce_stage_mask(newPass);
                break;

            case texture_usage::storage_write:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                outBarrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                outBarrier.dstStageMask = deduce_stage_mask(newPass);
                break;

            case texture_usage::transfer_source:
                OBLO_ASSERT(newPass == pass_kind::transfer);
                outBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                outBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                outBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;

            case texture_usage::transfer_destination:
                OBLO_ASSERT(newPass == pass_kind::transfer);
                outBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                outBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                outBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                break;

            case texture_usage::present:
                outBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                outBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                break;

            default:
                OBLO_ASSERT(false);
                break;
            };
        }
    }

    struct image_layout_tracker::image_layout
    {
        VkImageLayout layout;
        VkAccessFlags2 accessMask;
        VkPipelineStageFlags2 stageMask;
        VkImage image;
        VkImageSubresourceRange range;
    };

    VkImageLayout image_layout_tracker::deduce_layout(texture_usage usage)
    {
        VkImageMemoryBarrier2 b{};
        deduce_barrier(pass_kind::none, usage, b);
        return b.newLayout;
    }

    image_layout_tracker::image_layout_tracker() = default;

    image_layout_tracker::~image_layout_tracker() = default;

    void image_layout_tracker::start_tracking(handle_type handle, const texture& t)
    {
        VkImageAspectFlags aspectMask = 0;

        if (is_depth_format(t.initializer.format))
        {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (has_stencil(t.initializer.format))
            {
                aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else
        {
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        m_state.emplace(handle,
            image_layout{
                .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .image = t.image,
                .range =
                    {
                        .aspectMask = aspectMask,
                        .baseMipLevel = 0,
                        .levelCount = t.initializer.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount = t.initializer.arrayLayers,
                    },
            });
    }

    bool image_layout_tracker::add_transition(
        VkImageMemoryBarrier2& outBarrier, handle_type handle, pass_kind pass, texture_usage usage)
    {
        OBLO_ASSERT(pass != pass_kind::none || usage == texture_usage::present);

        auto* const state = m_state.try_find(handle);
        OBLO_ASSERT(state);

        if (!state)
        {
            return false;
        }

        const auto oldLayout = state->layout;
        const auto srcAccessMask = state->accessMask;
        const auto srcStageMask = state->stageMask;

        outBarrier = VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = srcStageMask,
            .srcAccessMask = srcAccessMask,
            .oldLayout = oldLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = state->image,
            .subresourceRange = state->range,
        };

        deduce_barrier(pass, usage, outBarrier);

        state->accessMask = outBarrier.dstAccessMask;
        state->stageMask = outBarrier.dstStageMask;
        state->layout = outBarrier.newLayout;

        return true;
    }

    void image_layout_tracker::clear()
    {
        m_state.clear();
    }

    expected<VkImageLayout> image_layout_tracker::try_get_layout(handle_type handle) const
    {
        auto* const state = m_state.try_find(handle);

        if (!state)
        {
            return unspecified_error;
        }

        return state->layout;
    }
}