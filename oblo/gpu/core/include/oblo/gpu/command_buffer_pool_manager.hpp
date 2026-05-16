#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/forward.hpp>

namespace oblo::gpu
{
    class command_buffer_pool_manager
    {
    public:
        command_buffer_pool_manager();
        command_buffer_pool_manager(const command_buffer_pool_manager&) = delete;
        command_buffer_pool_manager(command_buffer_pool_manager&&) noexcept = delete;
        ~command_buffer_pool_manager();

        command_buffer_pool_manager& operator=(const command_buffer_pool_manager&) = delete;
        command_buffer_pool_manager& operator=(command_buffer_pool_manager&&) noexcept = delete;

        void init(gpu_instance& gpu, u32 commandBuffersPerPool);
        void shutdown();

        result<hptr<command_buffer>> allocate_command_buffer();

        void frame_submitted(u64 submitIndex);

    private:
        struct used_command_buffer_pool;

    private:
        result<h32<gpu::command_buffer_pool>> get_or_create_current_pool();

    private:
        gpu_instance* m_gpu{};
        deque<used_command_buffer_pool> m_usedPools;
        h32<command_buffer_pool> m_currentPool{};
        u32 m_commandBuffersPerPool{};
    };
}