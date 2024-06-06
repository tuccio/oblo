#pragma once

#include <ostream>

namespace oblo
{
    template <typename Graph, typename F>
    void write_graphviz_dot(std::ostream& os, const Graph& g, F&& f)
    {
        os << "digraph G {\n";

        for (const auto v : g.get_vertices())
        {
            os << "  "
               << "v" << v.value << "[ label=\"" << f(v) << "\" ]\n";
        }

        for (const auto e : g.get_edges())
        {
            os << "  "
               << "v" << g.get_source(e).value << " -> "
               << "v" << g.get_destination(e).value << "\n";
        }

        os << "}\n";
    }
}