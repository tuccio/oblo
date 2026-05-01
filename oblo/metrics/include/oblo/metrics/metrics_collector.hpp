#pragma once

#include <oblo/metrics/async_metrics.hpp>
#include <oblo/metrics/metrics_module.hpp>

namespace oblo
{
    class metrics_collector
    {
    public:
        static OBLO_METRICS_API metrics_collector create_from_module();

        metrics_collector() = default;

        metrics_collector(const metrics_collector&) = delete;
        metrics_collector(metrics_collector&&) noexcept = default;

        explicit metrics_collector(metrics_module* module) : m_module{module} {}

        metrics_collector& operator=(const metrics_collector&) = delete;
        metrics_collector& operator=(metrics_collector&&) noexcept = default;

        bool is_collecting()
        {
            return m_module && m_module->is_collecting();
        }

        void push_entry(async_metrics_entry&& e)
        {
            m_entries.emplace_back(std::move(e));
        }

        template <typename T>
        void push_data(const T& data)
        {
            set_metrics_data_sync<T>(m_entries.emplace_back(), data);
        }

        void flush()
        {
            if (m_module && !m_entries.empty())
            {
                async_metrics m;
                m.init(std::move(m_entries));

                m_module->push_metrics(std::move(m));
            }
            else
            {
                m_entries.clear();
            }
        }

    private:
        metrics_module* m_module{};
        dynamic_array<async_metrics_entry> m_entries;
    };
}