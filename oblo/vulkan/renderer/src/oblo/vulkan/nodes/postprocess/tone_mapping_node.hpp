#pragma once

#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo::vk
{
    struct tone_mapping_node
    {
        resource<texture> inHDR;
        resource<texture> outLDR;

        h32<compute_pass> toneMappingPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}