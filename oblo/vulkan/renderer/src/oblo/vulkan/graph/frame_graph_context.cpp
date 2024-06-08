#include <oblo/vulkan/graph/frame_graph_context.hpp>

#include <oblo/vulkan/graph/frame_graph.hpp>

namespace oblo::vk
{
    frame_graph_build_context::frame_graph_build_context(frame_graph& frameGraph) : m_frameGraph{frameGraph} {}

    void* frame_graph_build_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }

    frame_graph_execute_context::frame_graph_execute_context(frame_graph& frameGraph) : m_frameGraph{frameGraph} {}

    void* frame_graph_execute_context::access_storage(h32<frame_graph_pin_storage> handle) const
    {
        return m_frameGraph.access_storage(handle);
    }
}