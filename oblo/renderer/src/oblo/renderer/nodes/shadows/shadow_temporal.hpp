#pragma once

#include <oblo/core/types.hpp>
#include <oblo/renderer/data/raytraced_shadow_config.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct shadow_temporal
    {
        pin::texture inShadow;
        pin::texture inShadowMean;
        pin::texture inHistory;

        pin::texture outFiltered;

        pin::texture inDisocclusionMask;
        pin::texture inMotionVectors;

        h32<compute_pass> temporalPass;
        h32<compute_pass_instance> temporalPassInstance;

        data<raytraced_shadow_config> inConfig;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}