#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/forward.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/enums.hpp>

namespace oblo
{
    class dynamic_buffer
    {
    public:
        dynamic_buffer() = default;
        dynamic_buffer(const dynamic_buffer&) = delete;
        dynamic_buffer(dynamic_buffer&&) noexcept = delete;
        ~dynamic_buffer();

        dynamic_buffer& operator=(const dynamic_buffer&) = delete;
        dynamic_buffer& operator=(dynamic_buffer&&) noexcept = delete;

        void init(gpu::gpu_instance& ctx, flags<gpu::buffer_usage> usage, gpu::memory_properties memoryProperties);
        void shutdown();

        void clear();
        void clear_and_shrink();

        void resize(hptr<gpu::command_buffer> cmd, u32 size);
        void resize_discard(u32 size);

        void reserve(hptr<gpu::command_buffer> cmd, u32 size);
        void reserve_discard(u32 size);

        gpu::buffer_range get_buffer() const;
        u32 get_capacity() const;

        u32 get_used_bytes() const;

    private:
        bool reserve_impl(hptr<gpu::command_buffer> cmd, u32 size);

    private:
        gpu::gpu_instance* m_ctx{};
        h32<gpu::buffer> m_buffer{};
        u32 m_capacity{};
        u32 m_usedBytes{};
        flags<gpu::buffer_usage> m_usage{};
        gpu::memory_properties m_memoryProperties;
    };
}