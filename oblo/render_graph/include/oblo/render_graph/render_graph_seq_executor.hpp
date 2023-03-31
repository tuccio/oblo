#pragma once

#include <oblo/render_graph/render_graph_node.hpp>
#include <vector>

namespace oblo
{
    class render_graph_builder_impl;

    class render_graph_seq_executor
    {
        friend class render_graph_builder_impl;

    public:
        void execute(void* context) const;

    private:
        struct node
        {
            void* ptr;
            render_graph_execute execute;
        };

        std::vector<node> m_nodes;
    };
}