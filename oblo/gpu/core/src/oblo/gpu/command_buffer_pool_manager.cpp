#include <oblo/gpu/command_buffer_pool_manager.hpp>

#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>

namespace oblo::gpu
{
    struct command_buffer_pool_manager::used_command_buffer_pool
    {
        h32<command_buffer_pool> pool;
        u64 submitIndex;
    };

    command_buffer_pool_manager::command_buffer_pool_manager() = default;

    command_buffer_pool_manager::~command_buffer_pool_manager() = default;

    void command_buffer_pool_manager::init(gpu_instance& gpu, u32 commandBuffersPerPool)
    {
        m_gpu = &gpu;
        m_commandBuffersPerPool = commandBuffersPerPool;
    }

    void command_buffer_pool_manager::shutdown()
    {
        if (m_currentPool)
        {
            m_gpu->destroy_next_frame(m_currentPool);
            m_currentPool = {};
        }

        for (const auto& used : m_usedPools)
        {
            m_gpu->destroy_deferred(used.pool, used.submitIndex);
        }

        m_usedPools.clear();
    }

    result<hptr<command_buffer>> command_buffer_pool_manager::allocate_command_buffer()
    {
        const expected pool = get_or_create_current_pool();

        if (!pool)
        {
            return pool.error();
        }

        hptr<command_buffer> cmd[1];
        const expected e = m_gpu->fetch_command_buffers(*pool, cmd);

        if (!e)
        {
            return e.error();
        }

        return cmd[0];
    }

    void command_buffer_pool_manager::frame_submitted(u64 submitIndex)
    {
        if (m_currentPool)
        {
            m_usedPools.emplace_back(m_currentPool, submitIndex);
            m_currentPool = {};
        }
    }

    result<h32<gpu::command_buffer_pool>> command_buffer_pool_manager::get_or_create_current_pool()
    {
        if (m_currentPool)
        {
            return m_currentPool;
        }

        // Check if we can reuse a pool, otherwise create one

        if (!m_usedPools.empty() && m_gpu->is_submit_done(m_usedPools.front().submitIndex))
        {
            m_currentPool = m_usedPools.front().pool;
            m_usedPools.pop_front();

            m_gpu->reset_command_buffer_pool(m_currentPool).assert_value();
        }
        else
        {
            const expected newPool = m_gpu->create_command_buffer_pool({
                .queue = m_gpu->get_universal_queue(),
                .numCommandBuffers = m_commandBuffersPerPool,
            });

            if (!newPool)
            {
                return newPool.error();
            }

            m_currentPool = *newPool;
        }

        OBLO_ASSERT(m_currentPool);

        return m_currentPool;
    }

}