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
        pin::buffer inSkyboxSettingsBuffer;

        pin::buffer inLightBuffer;
        pin::buffer inLightConfig;

        pin::buffer inSurfelsGrid;
        pin::buffer inSurfelsData;
    };
}