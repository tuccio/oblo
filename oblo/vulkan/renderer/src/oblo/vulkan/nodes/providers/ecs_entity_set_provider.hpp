#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct ecs_entity_set_provider
    {
        resource<buffer> outEntitySet;

        std::span<const staging_buffer_span> stagedData;

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}