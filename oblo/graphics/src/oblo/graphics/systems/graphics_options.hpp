#pragma once

#include <oblo/core/types.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>

namespace oblo
{
    template <>
    struct option_traits<"r.isRayTracingEnabled">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "b01a7290-4f14-4b5c-9693-3b748bd9f45a"_uuid,
            .name = "Enable Ray-Tracing",
            .category = "Graphics",
            .defaultValue = property_value_wrapper{true},
        };
    };

    template <>
    struct option_traits<"r.gi.maxSurfels">
    {
        using type = u32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::u32,
            .id = "230f65b2-3049-455a-aefd-fee4a6567d75"_uuid,
            .name = "Max Surfels",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{1u << 16},
            .minValue = property_value_wrapper{1u << 10},
        };
    };

    template <>
    struct option_traits<"r.gi.gridCellSize">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "7896ff6e-0291-4548-a113-83da75cda1cd"_uuid,
            .name = "Grid Cell Size",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{1.f},
            .minValue = property_value_wrapper{0.1f},
        };
    };

    struct vulkan_options
    {
        option_proxy<"r.isRayTracingEnabled"> isRayTracingEnabled;
    };

    struct surfels_gi_options
    {
        option_proxy<"r.gi.maxSurfels"> maxSurfels;
        option_proxy<"r.gi.gridCellSize"> gridCellSize;
    };
}