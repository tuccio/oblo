#pragma once

#include <oblo/core/string/debug_label.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_pool.hpp>

#include <span>

#include <vulkan/vulkan_core.h>

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
        void shutdown();

        void on_first_frame();

        h32<resident_texture> acquire();
        void set_texture(h32<resident_texture> h, VkImageView imageView, VkImageLayout layout);

        h32<resident_texture> add(const texture_resource& texture, const debug_label& debugName);
        void remove(h32<resident_texture> texture);

        std::span<const VkDescriptorImageInfo> get_textures2d_info() const;

        u32 get_max_descriptor_count() const;

        void flush_uploads(VkCommandBuffer commandBuffer);

    private:
        struct pending_texture_upload;

    private:
        bool create(const texture_resource& texture, resident_texture& out, const debug_label& debugName);
        void set_texture(h32<resident_texture> h, const resident_texture& residentTexture, VkImageLayout layout);

    private:
        vulkan_context* m_vkCtx{};
        staging_buffer* m_staging{};
        handle_pool<u32, 4> m_handlePool;

        dynamic_array<VkDescriptorImageInfo> m_imageInfo;
        dynamic_array<resident_texture> m_textures;
        dynamic_array<pending_texture_upload> m_pendingUploads;
    };
}