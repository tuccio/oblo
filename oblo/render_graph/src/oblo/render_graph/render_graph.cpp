#include <oblo/render_graph/render_graph.hpp>

namespace oblo
{
    void* render_graph::find_node(type_id type)
    {
        for (const auto& node : m_nodes)
        {
            if (node.typeId == type)
            {
                return node.ptr;
            }
        }

        return nullptr;
    }

    void* render_graph::find_input(std::string_view name, type_id type)
    {
        for (const auto& input : m_inputs)
        {
            if (input.name == name && input.typeId == type)
            {
                return input.ptr;
            }
        }

        return nullptr;
    }
}