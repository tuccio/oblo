#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/ecs/systems/system_descriptor.hpp>

#include <vector>

namespace oblo::ecs
{
    class system;
    class system_seq_executor;
    struct system_descriptor;

    class system_graph
    {
    public:
        system_graph();
        system_graph(const system_graph&);
        system_graph(system_graph&&) noexcept;

        ~system_graph();

        system_graph& operator=(const system_graph&);
        system_graph& operator=(system_graph&&) noexcept;

        h32<system> add_system(const system_descriptor& desc);

        template <typename T>
        h32<system> add_system();

        void add_edge(h32<system> from, h32<system> to);

        system_seq_executor instantiate() const;

    private:
        struct system_data;
        std::vector<system_data> m_systems;
    };

    template <typename T>
    h32<system> system_graph::add_system()
    {
        system_descriptor desc{
            .create = []() -> void* { return new T{}; },
            .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
        };

        desc.update = [](void* ptr, const system_update_context* ctx) { static_cast<T*>(ptr)->update(*ctx); };

        if constexpr (requires(T& s, const system_update_context& ctx) { s.first_update(ctx); })
        {
            desc.firstUpdate = [](void* ptr, const system_update_context* ctx)
            { static_cast<T*>(ptr)->first_update(*ctx); };
        }
        else
        {
            desc.firstUpdate = desc.update;
        }

        return add_system(desc);
    }
}