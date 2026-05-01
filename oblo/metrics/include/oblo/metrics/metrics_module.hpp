#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/thread/future.hpp>
#include <oblo/core/thread/spin_lock.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/metrics/async_metrics.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class metrics_module final : public module_interface
    {
    public:
        OBLO_METRICS_API bool startup(const module_initializer& initializer) override;
        OBLO_METRICS_API void shutdown() override;
        OBLO_METRICS_API bool finalize() override;

        bool is_collecting() const;

        OBLO_METRICS_API void start_collecting();
        OBLO_METRICS_API void stop_collecting();

        OBLO_METRICS_API void collect_metrics(dynamic_array<async_metrics>& out);

        OBLO_METRICS_API void push_metrics(async_metrics m);

    private:
        bool m_isCollecting{};
        dynamic_array<async_metrics> m_metrics;
        mutable spin_lock m_lock;
    };

    inline bool metrics_module::is_collecting() const
    {
        m_lock.lock();
        const bool collecting = m_isCollecting;
        m_lock.unlock();

        return collecting;
    }
}