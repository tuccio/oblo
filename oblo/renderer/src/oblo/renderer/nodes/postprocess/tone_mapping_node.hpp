#pragma once

#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct tone_mapping_node
    {
        pin::texture inHDR;
        pin::texture outLDR;

        h32<compute_pass> toneMappingPass;
        h32<compute_pass_instance> toneMappingPassInstance;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}