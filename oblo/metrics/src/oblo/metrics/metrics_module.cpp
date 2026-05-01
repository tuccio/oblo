#include <oblo/metrics/metrics_module.hpp>

#include <oblo/metrics/metrics_collector.hpp>
#include <oblo/modules/module_manager.hpp>

#include <mutex>

namespace oblo
{
    bool metrics_module::startup(const module_initializer&)
    {
        return true;
    }

    void metrics_module::shutdown() {}

    bool metrics_module::finalize()
    {
        return true;
    }

    void metrics_module::start_collecting()
    {
        const std::scoped_lock lock{m_lock};
        m_isCollecting = true;
        m_metrics.clear();
    }

    void metrics_module::stop_collecting()
    {
        const std::scoped_lock lock{m_lock};
        m_isCollecting = false;
    }

    void metrics_module::collect_metrics(dynamic_array<async_metrics>& out)
    {
        const std::scoped_lock lock{m_lock};

        out.reserve(out.size() + m_metrics.size());
        for (auto& m : m_metrics)
        {
            out.emplace_back(std::move(m));
        }

        m_metrics.clear();
    }

    void metrics_module::push_metrics(async_metrics m)
    {
        const std::scoped_lock lock{m_lock};

        if (m_isCollecting)
        {
            m_metrics.emplace_back(std::move(m));
        }
    }

    metrics_collector metrics_collector::create_from_module()
    {
        return metrics_collector{module_manager::get().find<metrics_module>()};
    }
}
