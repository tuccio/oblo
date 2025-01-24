#pragma once

#include <oblo/core/string/hashed_string_view.hpp>

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
}