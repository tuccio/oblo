#pragma once

#include <oblo/core/types.hpp>

#include <vector>

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
        std::vector<system_info> m_systems;
        bool m_firstUpdate{true};
    };
}