#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_pool.hpp>
#include <oblo/core/string/debug_label.hpp>
#include <oblo/gpu/forward.hpp>

#include <span>

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

        bool init(gpu::gpu_instance& gpu, gpu::staging_buffer& staging);
        void shutdown();

        void on_first_frame();

        h32<resident_texture> acquire();
        bool set_texture(h32<resident_texture> h, const texture_resource& texture, const debug_label& debugName);
        void set_external_texture(h32<resident_texture> h, h32<gpu::image> image, gpu::image_resource_state state);

        h32<resident_texture> add(const texture_resource& texture, const debug_label& debugName);
        void remove(h32<resident_texture> texture);

        void flush_uploads(hptr<gpu::command_buffer> commandBuffer);

        void update_texture_bind_groups() const;

        u32 get_used_textures_slots() const;

    private:
        struct pending_texture_upload;

    private:
        bool create_texture(
            const texture_resource& texture, gpu::bindless_image_descriptor& out, const debug_label& debugName);

        void set_texture_impl(
            h32<resident_texture> h, const gpu::bindless_image_descriptor& residentTexture, bool isOwned);

    private:
        gpu::gpu_instance* m_gpu{};
        gpu::staging_buffer* m_staging{};
        handle_pool<u32, 4> m_handlePool;

        u32 m_usedSlots{};

        dynamic_array<gpu::bindless_image_descriptor> m_textures;
        dynamic_array<bool> m_isOwned;
        dynamic_array<pending_texture_upload> m_pendingUploads;
    };
}