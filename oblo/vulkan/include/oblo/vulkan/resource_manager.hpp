#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace oblo::vk
{
    class command_buffer_state;
    struct texture;

    class resource_manager
    {
    public:
        resource_manager();
        resource_manager(const resource_manager&) = delete;
        resource_manager(resource_manager&&) = delete;
        resource_manager& operator=(const resource_manager&) = delete;
        resource_manager& operator=(resource_manager&&) = delete;
        ~resource_manager();

        handle<texture> register_texture(const texture& texture, VkImageLayout currentLayout);
        void unregister_texture(handle<texture> handle);

        const texture* try_find(handle<texture> handle) const;
        texture* try_find(handle<texture> handle);

        const texture& get(handle<texture> handle) const;
        texture& get(handle<texture> handle);

        bool commit(command_buffer_state& commandBufferState, VkCommandBuffer preparationBuffer);

    private:
        struct stored_texture;

    private:
        u32 m_lastTextureId{};
        flat_dense_map<handle<texture>, stored_texture> m_textures;
    };

    class command_buffer_state
    {
    public:
        command_buffer_state() = default;
        command_buffer_state(const command_buffer_state&) = delete;
        command_buffer_state(command_buffer_state&&) noexcept = default;
        command_buffer_state& operator=(const command_buffer_state&) = delete;
        command_buffer_state& operator=(command_buffer_state&&) noexcept = default;

        void set_starting_layout(handle<texture> handle, VkImageLayout currentLayout);

        void add_pipeline_barrier(const resource_manager& resourceManager,
                                  handle<texture> handle,
                                  VkCommandBuffer commandBuffer,
                                  VkImageLayout newLayout);

        void clear();

        bool has_incomplete_transitions() const;

    private:
        struct image_transition
        {
            handle<texture> handle;
            VkImageLayout newLayout;
            VkFormat format;
            VkImageAspectFlags aspectMask;
            u32 layerCount;
            u32 mipLevels;
        };

        friend class resource_manager;

    private:
        flat_dense_map<handle<texture>, VkImageLayout> m_transitions;
        std::vector<image_transition> m_incompleteTransitions;
    };
}
