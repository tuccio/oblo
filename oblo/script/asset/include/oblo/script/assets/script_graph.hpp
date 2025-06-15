#pragma once

#include <oblo/nodes/node_graph.hpp>

namespace oblo
{
    class script_graph : public node_graph
    {
    public:
        script_graph() = default;
        script_graph(const script_graph&) = delete;
        script_graph(script_graph&&) noexcept = default;

        script_graph& operator=(const script_graph&) = delete;
        script_graph& operator=(script_graph&&) noexcept = default;

        ~script_graph() = default;
    };
}