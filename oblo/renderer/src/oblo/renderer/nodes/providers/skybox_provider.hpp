#pragma once

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/vulkan/data/skybox_settings.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

namespace oblo
{
    class texture;
}

namespace oblo
{
    struct resident_texture;

    struct skybox_provider
    {
        data<resource_ptr<oblo::texture>> inSkyboxResource;
        data<skybox_settings> inSkyboxSettings;

        resource<buffer> outSkyboxSettingsBuffer;

        void build(const frame_graph_build_context& ctx);
    };
}