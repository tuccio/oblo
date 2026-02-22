#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_pool.hpp>
#include <oblo/core/string/debug_label.hpp>
#include <oblo/gpu/forward.hpp>

#include <span>

#include <vulkan/vulkan_core.h>

namespace oblo::gpu::vk
{
    class vulkan_instance;
}

namespace oblo
{
    class texture;
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

        bool init(gpu::vk::vulkan_instance& vkCtx, gpu::gpu_queue_context& queueCtx, gpu::staging_buffer& staging);
        void shutdown();

        void on_first_frame();

        h32<resident_texture> acquire();
        bool set_texture(h32<resident_texture> h, const texture_resource& texture, const debug_label& debugName);
        void set_texture(h32<resident_texture> h, VkImageView view, VkImageLayout layout);

        h32<resident_texture> add(const texture_resource& texture, const debug_label& debugName);
        void remove(h32<resident_texture> texture);

        std::span<const VkDescriptorImageInfo> get_textures2d_info() const;

        u32 get_max_descriptor_count() const;

        void flush_uploads(hptr<gpu::command_buffer> commandBuffer);

    private:
        struct pending_texture_upload;

    private:
        bool create(const texture_resource& texture, resident_texture& out, const debug_label& debugName);
        void set_texture(h32<resident_texture> h, const resident_texture& residentTexture, VkImageLayout layout);

    private:
        gpu::vk::vulkan_instance* m_vkCtx{};
        gpu::gpu_queue_context* m_queueCtx{};
        gpu::staging_buffer* m_staging{};
        handle_pool<u32, 4> m_handlePool;

        dynamic_array<VkDescriptorImageInfo> m_imageInfo;
        dynamic_array<resident_texture> m_textures;
        dynamic_array<pending_texture_upload> m_pendingUploads;
    };
}