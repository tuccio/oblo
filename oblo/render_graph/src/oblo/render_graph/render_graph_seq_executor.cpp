#include <oblo/render_graph/render_graph_seq_executor.hpp>

namespace oblo
{
    bool render_graph_seq_executor::initialize(void* context) const
    {
        usize initialized = 0;

        for (const auto node : m_nodes)
        {
            if (node.initialize)
            {
                if (!node.initialize(node.ptr, context))
                {
                    shutdown_n(context, initialized);
                    return false;
                }
            }

            ++initialized;
        }

        return true;
    }

    void render_graph_seq_executor::execute(void* context) const
    {
        for (const auto node : m_nodes)
        {
            node.execute(node.ptr, context);
        }
    }

    void render_graph_seq_executor::shutdown(void* context) const
    {
        shutdown_n(context, m_nodes.size());
    }

    void render_graph_seq_executor::shutdown_n(void* context, usize offset) const
    {
        const auto reverseOffset = m_nodes.size() - offset;

        for (auto it = m_nodes.rbegin() + reverseOffset; it != m_nodes.rend(); ++it)
        {
            if (auto& node = *it; node.shutdown)
            {
                node.shutdown(node.ptr, context);
            }
        }
    }
}