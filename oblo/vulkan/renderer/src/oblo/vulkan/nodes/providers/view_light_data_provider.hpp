#pragma once

#include <oblo/vulkan/data/light_data.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
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