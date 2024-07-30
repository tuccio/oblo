#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct shadow_filter
    {
        resource<texture> inSource;

        resource<texture> outFiltered;

        h32<compute_pass> filterPass;

        u32 passIndex;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}