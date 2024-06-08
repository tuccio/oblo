#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    class frame_graph;
    struct frame_graph_pin_storage;

    class frame_graph_init_context
    {
    };

    class frame_graph_build_context
    {
    public:
        frame_graph_build_context(frame_graph& frameGraph);

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph& m_frameGraph;
    };

    class frame_graph_execute_context
    {
    public:
        frame_graph_execute_context(frame_graph& frameGraph);

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph& m_frameGraph;
    };
}