#pragma once

#include <oblo/resource/resource_ptr.hpp>
#include <oblo/renderer/data/skybox_settings.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

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