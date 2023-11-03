#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/types.hpp>


#include <vulkan/vulkan.h>

#include <vector>

namespace oblo::vk
{
    class allocator;
    class command_buffer_state;
    struct buffer;
    struct buffer_initializer;
    struct image_initializer;
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

        h32<texture> register_texture(const texture& texture, VkImageLayout currentLayout);
        void unregister_texture(h32<texture> handle);

        h32<buffer> register_buffer(const buffer& buffer);
        void unregister_buffer(h32<buffer> handle);

        h32<texture> create(allocator& allocator, const image_initializer& initializer);
        h32<buffer> create(allocator& allocator, const buffer_initializer& initializer);

        void destroy(allocator& allocator, h32<texture> texture);
        void destroy(allocator& allocator, h32<buffer> texture);

        const texture* try_find(h32<texture> handle) const;
        texture* try_find(h32<texture> handle);

        const buffer* try_find(h32<buffer> handle) const;
        buffer* try_find(h32<buffer> handle);

        const texture& get(h32<texture> handle) const;
        texture& get(h32<texture> handle);

        const buffer& get(h32<buffer> handle) const;
        buffer& get(h32<buffer> handle);

        bool commit(command_buffer_state& commandBufferState, VkCommandBuffer preparationBuffer);

    private:
        struct stored_buffer;
        struct stored_texture;

    private:
        h32_flat_pool_dense_map<buffer, stored_buffer> m_buffers;
        h32_flat_pool_dense_map<texture, stored_texture> m_textures;
    };

    class command_buffer_state
    {
    public:
        command_buffer_state() = default;
        command_buffer_state(const command_buffer_state&) = delete;
        command_buffer_state(command_buffer_state&&) noexcept = default;
        command_buffer_state& operator=(const command_buffer_state&) = delete;
        command_buffer_state& operator=(command_buffer_state&&) noexcept = default;

        void set_starting_layout(h32<texture> handle, VkImageLayout currentLayout);

        void add_pipeline_barrier(const resource_manager& resourceManager,
            h32<texture> handle,
            VkCommandBuffer commandBuffer,
            VkImageLayout newLayout);

        void clear();

        bool has_incomplete_transitions() const;

    private:
        struct image_transition
        {
            h32<texture> handle;
            VkImageLayout newLayout;
            VkFormat format;
            VkImageAspectFlags aspectMask;
            u32 layerCount;
            u32 mipLevels;
        };

        friend class resource_manager;

    private:
        using extractor = h32_flat_pool_dense_map<texture>::extractor_type;

        flat_dense_map<h32<texture>, VkImageLayout, extractor> m_transitions;
        std::vector<image_transition> m_incompleteTransitions;
    };
}
