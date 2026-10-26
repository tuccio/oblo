#pragma once

#include <oblo/core/span.hpp>
#include <oblo/renderer/data/light_data.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct view_light_data_provider
    {
        pin::data<span<const light_data>> inLights;
        pin::buffer inSkyboxSettingsBuffer;

        pin::buffer inLightBuffer;
        pin::buffer inLightConfig;
    };
}