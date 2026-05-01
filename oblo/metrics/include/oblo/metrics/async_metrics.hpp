#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/thread/future.hpp>
#include <oblo/core/type_id.hpp>

#include <span>

namespace oblo
{
    struct async_metrics_entry;

    class async_metrics
    {
    public:
        using entry = async_metrics_entry;

    public:
        OBLO_METRICS_API void init(dynamic_array<entry> entries);

        OBLO_METRICS_API std::span<entry> get_entries();
        OBLO_METRICS_API std::span<const entry> get_entries() const;

        OBLO_METRICS_API void update();

        OBLO_METRICS_API bool is_done() const;

    private:
        dynamic_array<entry> m_entries;
        bool m_isDone{};
    };

    struct async_metrics_entry
    {
        type_id type;
        future<dynamic_array<byte>> download;
    };

    template <typename T, typename... Args>
    void set_metrics_data_sync(async_metrics_entry& e, Args&&... args)
    {
        dynamic_array<byte> data;
        data.resize_default(sizeof(T));

        new (data.data()) T{std::forward<Args>(args)...};

        promise<dynamic_array<byte>> p;
        p.init();
        p.set_value(std::move(data));

        e.download = future{p};
        e.type = get_type_id<T>();
    }
}