#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/gpu_temporary_aliases.hpp>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    struct buffer;
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

        h32<texture> create(gpu_allocator& allocator, const image_initializer& initializer);
        h32<buffer> create(gpu_allocator& allocator, const buffer_initializer& initializer);

        void destroy(gpu_allocator& allocator, h32<texture> texture);
        void destroy(gpu_allocator& allocator, h32<buffer> texture);

        const texture* try_find(h32<texture> handle) const;
        texture* try_find(h32<texture> handle);

        const buffer* try_find(h32<buffer> handle) const;
        buffer* try_find(h32<buffer> handle);

        const texture& get(h32<texture> handle) const;
        texture& get(h32<texture> handle);

        const buffer& get(h32<buffer> handle) const;
        buffer& get(h32<buffer> handle);

    private:
        struct stored_buffer;
        struct stored_texture;

    private:
        h32_flat_pool_dense_map<buffer, stored_buffer> m_buffers;
        h32_flat_pool_dense_map<texture, stored_texture> m_textures;
    };
}
