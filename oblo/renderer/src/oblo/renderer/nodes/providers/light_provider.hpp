#pragma once

#include <oblo/core/span.hpp>
#include <oblo/renderer/data/light_data.hpp>
#include <oblo/renderer/graph/node_common.hpp>
#include <oblo/renderer/graph/pins.hpp>

namespace oblo
{
    struct light_provider
    {
        pin::data<span<const light_data>> inOutLights;
        pin::buffer outLightData;
        pin::buffer outLightConfig;

        void build(const frame_graph_build_context& ctx)
        {
            ctx.empty_pass();

            const span lights = ctx.access(inOutLights);
            const u32 lightsCount = u32(lights.size());

            // TODO: We need a way of creating buffers without using them, and possibly discard them if they are not
            // used by anyone after
            ctx.create(outLightData,
                {
                    .size = u32(lightsCount * sizeof(light_data)),
                    .data = as_bytes(lights),
                },
                buffer_usage::storage_read);

            const light_config config{
                .lightsCount = lightsCount,
            };

            ctx.create(outLightConfig,
                {
                    .size = sizeof(light_config),
                    .data = as_bytes(span{&config, 1}),
                },
                buffer_usage::uniform);
        }
    };
}