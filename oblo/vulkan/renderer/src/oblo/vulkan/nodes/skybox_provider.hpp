#pragma once

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo
{
    class texture;
}
namespace oblo::vk
{
    struct resident_texture;

    struct skybox_provider
    {
        data<resource_ptr<oblo::texture>> inSkyboxResource;

        data<h32<resident_texture>> outSkyboxResidentTexture;

        void build(const frame_graph_build_context& ctx);
    };
}