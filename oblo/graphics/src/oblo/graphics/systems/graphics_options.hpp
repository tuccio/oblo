#pragma once

#include <oblo/core/types.hpp>
#include <oblo/options/option_traits.hpp>

namespace oblo
{
    struct graphics_options
    {
        bool isRayTracingEnabled{false};
        u32 maxSurfels{1u << 16};
        f32 gridCellSize{1.f};
    };

    template <>
    struct option_traits<&graphics_options::isRayTracingEnabled>
    {
        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "b01a7290-4f14-4b5c-9693-3b748bd9f45a"_uuid,
            .name = "Enable Ray-Tracing",
            .category = "Graphics",
            .defaultValue = property_value_wrapper{true},
        };
    };

    template <>
    struct option_traits<&graphics_options::maxSurfels>
    {
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
    struct option_traits<&graphics_options::gridCellSize>
    {
        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "7896ff6e-0291-4548-a113-83da75cda1cd"_uuid,
            .name = "Grid Cell Size",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{1.f},
            .minValue = property_value_wrapper{0.1f},
        };
    };
}