#pragma once

#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo
{
    template <typename>
    class resource_ptr;

    class texture;
}
namespace oblo::vk
{
    struct resident_texture;

    struct skybox_pass
    {
        data<vec2u> inResolution;
        data<h32<resident_texture>> inSkyboxResidentTexture;

        resource<texture> outShading;

        h32<compute_pass> skyboxPass;

        void init(const frame_graph_init_context& ctx);

        void build(const frame_graph_build_context& ctx);

        void execute(const frame_graph_execute_context& ctx);
    };
}