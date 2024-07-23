#pragma once

#include <oblo/vulkan/data/light_data.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <span>

namespace oblo::vk
{
    struct light_provider
    {
        data<std::span<const light_data>> inOutLights;
        resource<buffer> outLightData;
        resource<buffer> outLightConfig;

        void build(const frame_graph_build_context& builder)
        {
            const std::span lights = builder.access(inOutLights);
            const u32 lightsCount = u32(lights.size());

            builder.create(outLightData,
                {
                    .size = u32(lightsCount * sizeof(light_data)),
                    .data = std::as_bytes(lights),
                },
                pass_kind::graphics,
                buffer_usage::storage_read);

            const light_config config{
                .lightsCount = lightsCount,
            };

            builder.create(outLightConfig,
                {
                    .size = sizeof(light_config),
                    .data = std::as_bytes(std::span{&config, 1}),
                },
                pass_kind::graphics,
                buffer_usage::uniform);
        }
    };
}