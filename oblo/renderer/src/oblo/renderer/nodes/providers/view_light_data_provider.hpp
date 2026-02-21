#pragma once

#include <oblo/renderer/data/light_data.hpp>
#include <oblo/renderer/graph/forward.hpp>
#include <oblo/renderer/graph/pins.hpp>

#include <span>

namespace oblo
{
    struct view_light_data_provider
    {
        data<std::span<const light_data>> inLights;
        resource<buffer> inSkyboxSettingsBuffer;

        resource<buffer> inLightBuffer;
        resource<buffer> inLightConfig;

        resource<buffer> inSurfelsGrid;
        resource<buffer> inSurfelsData;
    };
}