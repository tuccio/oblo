#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/handle_pool.hpp>

#include <span>
#include <vector>

struct VkDescriptorImageInfo;

namespace oblo
{
    class texture;
}

namespace oblo::vk
{
    class staging_buffer;
    class vulkan_context;
    struct resident_texture;

    using texture_resource = oblo::texture;

    class texture_registry
    {
    public:
        texture_registry();
        texture_registry(const texture_registry&) = delete;
        texture_registry(texture_registry&&) noexcept = delete;
        ~texture_registry();

        texture_registry& operator=(const texture_registry&) = delete;
        texture_registry& operator=(texture_registry&&) noexcept = delete;

        bool init(vulkan_context& vkCtx, staging_buffer& staging);

        h32<resident_texture> add(staging_buffer& staging, const texture_resource& texture);
        void remove(h32<resident_texture> texture);

        std::span<const VkDescriptorImageInfo> get_textures2d_info() const;

        u32 get_max_descriptor_count() const;

    private:
        bool create(staging_buffer& staging, const texture_resource& texture, resident_texture& out);

    private:
        vulkan_context* m_vkCtx{};
        handle_pool<u32, 4> m_handlePool;

        std::vector<VkDescriptorImageInfo> m_imageInfo;
        std::vector<resident_texture> m_textures;
    };
}