#pragma once

#include <oblo/core/types.hpp>
#include <oblo/render_graph/render_graph_node.hpp>
#include <vector>

namespace oblo
{
    class render_graph_builder_impl;

    class render_graph_seq_executor
    {
        friend class render_graph_builder_impl;

    public:
        bool initialize(void* context) const;
        void execute(void* context) const;
        void shutdown(void* context) const;

    private:
        void shutdown_n(void* context, usize n) const;

    private:
        struct node
        {
            void* ptr;
            render_graph_initialize initialize;
            render_graph_execute execute;
            render_graph_shutdown shutdown;
        };

        std::vector<node> m_nodes;
    };
}