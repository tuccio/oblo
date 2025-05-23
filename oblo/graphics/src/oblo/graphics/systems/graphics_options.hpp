#pragma once

#include <oblo/core/types.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>

namespace oblo
{
    template <>
    struct option_traits<"r.gi.maxSurfels">
    {
        using type = u32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::u32,
            .id = "230f65b2-3049-455a-aefd-fee4a6567d75"_uuid,
            .name = "Max Surfels",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{1u << 18},
            .minValue = property_value_wrapper{1u},
        };
    };

    template <>
    struct option_traits<"r.gi.rayBudget">
    {
        using type = u32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::u32,
            .id = "d624c991-1f79-4c75-9c40-c342ee97be7e"_uuid,
            .name = "Ray Budget",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{1u << 19},
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
            .maxValue = property_value_wrapper{20.f},
        };
    };

    template <>
    struct option_traits<"r.gi.gridSizeX">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "1aaa319e-7249-4dc3-9ca0-71adf65d9a32"_uuid,
            .name = "Grid Width",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{32.f},
            .minValue = property_value_wrapper{1.f},
            .maxValue = property_value_wrapper{1024.f},
        };
    };

    template <>
    struct option_traits<"r.gi.gridSizeY">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "06201137-3847-4e16-a976-a68573f8fb20"_uuid,
            .name = "Grid Height",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{16.f},
            .minValue = property_value_wrapper{1.f},
            .maxValue = property_value_wrapper{1024.f},
        };
    };

    template <>
    struct option_traits<"r.gi.gridSizeZ">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "fb03d98f-ad8d-4de3-b86d-c465fd3210d0"_uuid,
            .name = "Grid Depth",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{32.f},
            .minValue = property_value_wrapper{1.f},
            .maxValue = property_value_wrapper{1024.f},
        };
    };

    template <>
    struct option_traits<"r.gi.multiplier">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "a9e802d0-85cc-47f1-a6cb-26b53ced57db"_uuid,
            .name = "GI Multiplier",
            .category = "Graphics/GI",
            .defaultValue = property_value_wrapper{1.f},
            .minValue = property_value_wrapper{0.f},
            .maxValue = property_value_wrapper{1024.f},
        };
    };

    struct surfels_gi_options
    {
        option_proxy<"r.gi.maxSurfels"> maxSurfels;
        option_proxy<"r.gi.rayBudget"> rayBudget;
        option_proxy<"r.gi.gridCellSize"> gridCellSize;
        option_proxy<"r.gi.gridSizeX"> gridSizeX;
        option_proxy<"r.gi.gridSizeY"> gridSizeY;
        option_proxy<"r.gi.gridSizeZ"> gridSizeZ;
        option_proxy<"r.gi.multiplier"> giMultiplier;
    };

    struct surfels_gi_config
    {
        u32 maxSurfels;
        u32 rayBudget;
        f32 gridCellSize;
        f32 gridSizeX;
        f32 gridSizeY;
        f32 gridSizeZ;
        f32 multiplier;
    };

    template <>
    struct option_traits<"r.rtao.bias">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "505db10a-5c00-4f49-ab6a-7b6f34626c57"_uuid,
            .name = "Distance Bias",
            .category = "Graphics/RTAO",
            .defaultValue = property_value_wrapper{1e-2f},
            .minValue = property_value_wrapper{0.f},
            .maxValue = property_value_wrapper{1.f},
        };
    };

    template <>
    struct option_traits<"r.rtao.maxDistance">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "19f4c130-879f-47f3-af3a-a4ad51d52856"_uuid,
            .name = "Max Distance",
            .category = "Graphics/RTAO",
            .defaultValue = property_value_wrapper{.5f},
            .minValue = property_value_wrapper{0.f},
            .maxValue = property_value_wrapper{1024.f},
        };
    };

    template <>
    struct option_traits<"r.rtao.maxHistoryWeight">
    {
        using type = f32;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::f32,
            .id = "d0caee84-21e6-422f-8def-3153e42fc9a6"_uuid,
            .name = "Max History Weight",
            .category = "Graphics/RTAO",
            .defaultValue = property_value_wrapper{.92f},
            .minValue = property_value_wrapper{0.f},
            .maxValue = property_value_wrapper{1},
        };
    };

    struct rtao_options
    {
        option_proxy<"r.rtao.bias"> bias;
        option_proxy<"r.rtao.maxDistance"> maxDistance;
        option_proxy<"r.rtao.maxHistoryWeight"> maxHistoryWeight;
    };

    struct rtao_config
    {
        f32 bias;
        f32 maxDistance;
        f32 maxHistoryWeight;
    };
}