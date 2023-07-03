#include <oblo/vulkan/resource_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>
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

        void add_pipeline_barrier_cmd(VkCommandBuffer commandBuffer,
                                      VkImageLayout oldLayout,
                                      VkImageLayout newLayout,
                                      VkImage image,
                                      VkFormat format,
                                      u32 layerCount,
                                      u32 mipLevels)
        {
            VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = 0,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange =
                    VkImageSubresourceRange{
                        .aspectMask = 0,
                        .baseMipLevel = 0,
                        .levelCount = mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount = layerCount,
                    },
            };

            VkPipelineStageFlags sourceStage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
            VkPipelineStageFlags destinationStage{0};

            if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || is_depth_format(format))
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                if (has_stencil(format))
                {
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            }
            else
            {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            switch (oldLayout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED: {
                barrier.srcAccessMask = 0;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                break;
            }

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            }

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            }

            default:
                break;
            }

            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            }

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            }

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;
            }

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: {
                destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;
            }

            default:
                unreachable();
            }

            vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    void resource_manager::register_image(VkImage image, VkImageLayout currentLayout)
    {
        [[maybe_unused]] const auto [it, ok] = m_states.emplace(image, currentLayout);
        OBLO_ASSERT(ok);
    }

    void resource_manager::unregister_image(VkImage image)
    {
        [[maybe_unused]] const auto n = m_states.erase(image);
        OBLO_ASSERT(n == 1);
    }

    bool resource_manager::commit(command_buffer_state& commandBufferState, VkCommandBuffer transitionsBuffer)
    {
        const bool anyTransition{commandBufferState.has_incomplete_transitions()};
        OBLO_ASSERT(!anyTransition || transitionsBuffer);

        // Fill up the transitions buffer with the incomplete transitions, which we can now check from global state
        for (const auto& pendingTransition : commandBufferState.m_incompleteTransitions)
        {
            const auto it = m_states.find(pendingTransition.image);
            OBLO_ASSERT(it != m_states.end(), "The image is not registered, unable to add transition");

            if (it != m_states.end())
            {
                const auto oldLayout = it->second;

                add_pipeline_barrier_cmd(transitionsBuffer,
                                         oldLayout,
                                         pendingTransition.newLayout,
                                         pendingTransition.image,
                                         pendingTransition.format,
                                         pendingTransition.layerCount,
                                         pendingTransition.mipLevels);
            }
        }

        // Update the global state
        for (const auto& [image, newLayout] : commandBufferState.m_transitions)
        {
            const auto it = m_states.find(image);

            if (it != m_states.end())
            {
                it->second = newLayout;
            }
        }

        return anyTransition;
    }

    void command_buffer_state::set_starting_layout(VkImage image, VkImageLayout currentLayout)
    {
        m_transitions.emplace(image, currentLayout);
    }

    void command_buffer_state::add_pipeline_barrier(VkCommandBuffer commandBuffer,
                                                    VkImageLayout newLayout,
                                                    const texture& texture)
    {
        const auto image = texture.image;
        const auto it = m_transitions.find(image);

        if (it == m_transitions.end())
        {
            m_transitions.emplace_hint(it, image, newLayout);

            m_incompleteTransitions.emplace_back(image_transition{
                .image = image,
                .newLayout = newLayout,
                .format = texture.format,
                .layerCount = texture.arrayLayers,
                .mipLevels = texture.mipLevels,
            });
        }
        else
        {
            const auto oldLayout = it->second;
            it->second = newLayout;

            add_pipeline_barrier_cmd(commandBuffer,
                                     oldLayout,
                                     newLayout,
                                     image,
                                     texture.format,
                                     texture.arrayLayers,
                                     texture.mipLevels);
        }
    }

    void command_buffer_state::clear()
    {
        m_transitions.clear();
        m_incompleteTransitions.clear();
    }

    bool command_buffer_state::has_incomplete_transitions() const
    {
        return !m_incompleteTransitions.empty();
    }
}