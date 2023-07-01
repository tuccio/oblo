#pragma once

#include <oblo/core/types.hpp>

#include <ankerl/unordered_dense.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace oblo::vk
{
    class command_buffer_state;
    struct texture;

    enum class image_state : u8
    {
        undefined,
        color_attachment,
        depth_attachment,
        depth_stencil_attachment,
    };

    class resource_manager
    {
    public:
        void register_image(VkImage image, VkImageLayout currentLayout);
        void unregister_image(VkImage image);

        bool commit(command_buffer_state& commandBufferState, VkCommandBuffer preparationBuffer);

    private:
        ankerl::unordered_dense::map<VkImage, VkImageLayout> m_states;
    };

    class command_buffer_state
    {
    public:
        command_buffer_state() = default;
        command_buffer_state(const command_buffer_state&) = delete;
        command_buffer_state(command_buffer_state&&) noexcept = default;
        command_buffer_state& operator=(const command_buffer_state&) = delete;
        command_buffer_state& operator=(command_buffer_state&&) noexcept = default;

        void set_starting_layout(VkImage image, VkImageLayout currentLayout);

        void add_pipeline_barrier(VkCommandBuffer commandBuffer,
                                  VkImageLayout newLayout,
                                  VkImage image,
                                  VkFormat format,
                                  u32 layerCount,
                                  u32 mipLevels);

        void add_pipeline_barrier(VkCommandBuffer commandBuffer, image_state newState, const texture& texture);

        void clear();

        bool has_incomplete_transitions() const;

    private:
        struct image_transition
        {
            image_state newState;
            VkImage image;
            VkImageLayout newLayout;
            VkFormat format;
            VkImageAspectFlags aspectMask;
            u32 layerCount;
            u32 mipLevels;
        };

        friend class resource_manager;

    private:
        ankerl::unordered_dense::map<VkImage, VkImageLayout> m_transitions;
        std::vector<image_transition> m_incompleteTransitions;
    };
}
