#include <oblo/metrics/async_metrics.hpp>

namespace oblo
{
    void async_metrics::init(dynamic_array<entry> entries)
    {
        m_entries = std::move(entries);
        m_isDone = false;
    }

    std::span<async_metrics::entry> async_metrics::get_entries()
    {
        return m_entries;
    }

    std::span<const async_metrics::entry> async_metrics::get_entries() const
    {
        return m_entries;
    }

    void async_metrics::update()
    {
        if (m_isDone)
        {
            return;
        }

        for (const auto& e : m_entries)
        {
            const auto r = e.download.try_get_result();

            if (!r.has_value() && r.error() == future_error::not_ready)
            {
                return;
            }
        }

        m_isDone = true;
    }

    bool async_metrics::is_done() const
    {
        return m_isDone;
    }
}