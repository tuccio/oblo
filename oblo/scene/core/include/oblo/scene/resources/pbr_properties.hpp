#pragma once

#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/scene/resources/material.hpp>

namespace oblo
{
    struct material_property_descriptor
    {
        hashed_string_view name;
        material_property_type type;
        material_data_storage defaultValue;
    };
}

namespace oblo::pbr
{
    constexpr auto Albedo{"Albedo"_hsv};
    constexpr auto AlbedoTexture{"Albedo Texture"_hsv};
    constexpr auto Roughness{"Roughness"_hsv};
    constexpr auto Metalness{"Metalness"_hsv};
    constexpr auto MetalnessRoughnessTexture{"Metalness/Roughness Texture"_hsv};
    constexpr auto Emissive{"Emissive"_hsv};
    constexpr auto EmissiveMultiplier{"Emissive Multiplier"_hsv};
    constexpr auto EmissiveTexture{"Emissive Texture"_hsv};
    constexpr auto NormalMapTexture{"Normal Map Texture"_hsv};
    constexpr auto IndexOfRefraction{"Index Of Refraction"_hsv};

    struct properties
    {
        material_property_descriptor albedo{
            .name = Albedo,
            .type = material_property_type::linear_color_rgb_f32,
            .defaultValue = make_material_data_storage(vec3::splat(1.f)),
        };

        material_property_descriptor albedoTexture{
            .name = AlbedoTexture,
            .type = material_property_type::texture,
            .defaultValue = make_material_data_storage(resource_ref<texture>{}),
        };
    };
}