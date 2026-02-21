#pragma once

#include <oblo/renderer/data/light_data.hpp>
#include <oblo/renderer/graph/node_common.hpp>
#include <oblo/renderer/graph/pins.hpp>

#include <span>

namespace oblo
{
    struct light_provider
    {
        data<std::span<const light_data>> inOutLights;
        resource<buffer> outLightData;
        resource<buffer> outLightConfig;

        void build(const frame_graph_build_context& ctx)
        {
            ctx.empty_pass();

            const std::span lights = ctx.access(inOutLights);
            const u32 lightsCount = u32(lights.size());

            // TODO: We need a way of creating buffers without using them, and possibly discard them if they are not
            // used by anyone after
            ctx.create(outLightData,
                {
                    .size = u32(lightsCount * sizeof(light_data)),
                    .data = std::as_bytes(lights),
                },
                buffer_usage::storage_read);

            const light_config config{
                .lightsCount = lightsCount,
            };

            ctx.create(outLightConfig,
                {
                    .size = sizeof(light_config),
                    .data = std::as_bytes(std::span{&config, 1}),
                },
                buffer_usage::uniform);
        }
    };
}