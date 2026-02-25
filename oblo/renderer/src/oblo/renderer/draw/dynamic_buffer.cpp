#include <oblo/renderer/draw/dynamic_buffer.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/gpu/gpu_instance.hpp>

namespace oblo
{
    dynamic_buffer::~dynamic_buffer()
    {
        OBLO_ASSERT(!m_buffer, "Shutdown should be explicit");
    }

    void dynamic_buffer::init(
        gpu::gpu_instance& ctx, flags<gpu::buffer_usage> usage, gpu::memory_properties memoryProperties)
    {
        m_ctx = &ctx;
        m_buffer = {};
        m_capacity = {};
        m_usedBytes = {};
        m_usage = usage;
        m_memoryProperties = memoryProperties;
    }

    void dynamic_buffer::shutdown()
    {
        if (m_buffer)
        {
            const auto submitIndex = m_ctx->get_submit_index();
            m_ctx->destroy_deferred(m_buffer, submitIndex);

            m_buffer = {};
            m_capacity = {};
            m_usedBytes = {};
        }

        m_ctx = {};
    }

    void dynamic_buffer::clear()
    {
        m_usedBytes = {};
    }

    void dynamic_buffer::clear_and_shrink()
    {
        if (m_buffer)
        {
            const auto submitIndex = m_ctx->get_submit_index();

            m_ctx->destroy_deferred(m_buffer, submitIndex);

            m_buffer = {};
            m_capacity = {};
            m_usedBytes = {};
        }
    }

    void dynamic_buffer::resize(hptr<gpu::command_buffer> cmd, u32 size)
    {
        reserve_impl(cmd, size);
        m_usedBytes = size;
    }

    void dynamic_buffer::resize_discard(u32 size)
    {
        reserve_discard(size);
        m_usedBytes = size;
    }

    bool dynamic_buffer::reserve_impl(hptr<gpu::command_buffer> cmd, u32 size)
    {
        if (size <= m_capacity)
        {
            return true;
        }

        const h32 oldBuffer = m_buffer;

        auto& gpu = *m_ctx;

        const expected newBuffer = gpu.create_buffer({
            .size = size,
            .memoryProperties = m_memoryProperties,
            .usages = m_usage,
        });

        if (!newBuffer)
        {
            return false;
        }

        if (cmd && m_usedBytes > 0)
        {
            const gpu::buffer_copy_descriptor region{.size = m_usedBytes};
            gpu.cmd_copy_buffer(cmd, oldBuffer, *newBuffer, {&region, 1});
        }

        if (oldBuffer)
        {
            m_ctx->destroy_deferred(oldBuffer, m_ctx->get_submit_index());
        }

        m_buffer = *newBuffer;
        m_capacity = size;

        return true;
    }

    void dynamic_buffer::reserve(hptr<gpu::command_buffer> cmd, u32 size)
    {
        reserve_impl(cmd, size);
    }

    void dynamic_buffer::reserve_discard(u32 size)
    {
        reserve_impl({}, size);
    }

    gpu::buffer_range dynamic_buffer::get_buffer() const
    {
        return {
            .buffer = m_buffer,
            .size = m_usedBytes,
        };
    }

    u32 dynamic_buffer::get_capacity() const
    {
        return m_capacity;
    }

    u32 dynamic_buffer::get_used_bytes() const
    {
        return m_usedBytes;
    }
}