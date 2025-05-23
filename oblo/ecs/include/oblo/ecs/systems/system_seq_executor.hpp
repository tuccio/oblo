#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/types.hpp>

namespace oblo::ecs
{
    struct system_descriptor;
    struct system_update_context;

    class system_seq_executor
    {
    public:
        system_seq_executor();
        system_seq_executor(const system_seq_executor&) = delete;
        system_seq_executor(system_seq_executor&&) noexcept;

        ~system_seq_executor();

        system_seq_executor& operator=(const system_seq_executor&) = delete;
        system_seq_executor& operator=(system_seq_executor&&) noexcept;

        void update(const system_update_context& ctx);
        void shutdown();

    private:
        friend class system_graph;

        void push(const system_descriptor& desc);
        void reserve(usize capacity);

    private:
        struct system_info;
        dynamic_array<system_info> m_systems;
        u64 m_modificationId{};
    };
}