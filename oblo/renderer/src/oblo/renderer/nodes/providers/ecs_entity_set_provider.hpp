#pragma once

#include <oblo/gpu/forward.hpp>
#include <oblo/renderer/data/render_world.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct ecs_entity_set_provider
    {
        pin::data<render_world> inRenderWorld;
        pin::buffer outEntitySet;
        h32<transfer_pass_instance> uploadPass;

        std::span<const gpu::staging_buffer_span> stagedData;

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}