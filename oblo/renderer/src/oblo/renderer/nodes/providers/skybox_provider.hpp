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
        pin::data<resource_ptr<oblo::texture>> inSkyboxResource;
        pin::data<skybox_settings> inSkyboxSettings;

        pin::buffer outSkyboxSettingsBuffer;

        void build(const frame_graph_build_context& ctx);
    };
}