#pragma once

#include <oblo/vulkan/data/render_world.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct ecs_entity_set_provider
    {
        data<render_world> inRenderWorld;
        resource<buffer> outEntitySet;
        h32<transfer_pass_instance> uploadPass;

        std::span<const staging_buffer_span> stagedData;

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}