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
        void init(dynamic_array<entry> entries);

        std::span<entry> get_entries();
        std::span<const entry> get_entries() const;

        void update();

        bool is_done() const;

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