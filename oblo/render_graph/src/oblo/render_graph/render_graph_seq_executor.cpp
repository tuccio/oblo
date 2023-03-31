#include <oblo/render_graph/render_graph_seq_executor.hpp>

namespace oblo
{
    void render_graph_seq_executor::execute(void* context) const
    {
        for (const auto node : m_nodes)
        {
            node.execute(node.ptr, context);
        }
    }
}