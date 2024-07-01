#pragma once

#include <string_view>

namespace oblo::pbr
{
    constexpr std::string_view Albedo{"Albedo"};
    constexpr std::string_view AlbedoTexture{"Albedo Texture"};
    constexpr std::string_view Roughness{"Roughness"};
    constexpr std::string_view Metalness{"Metalness"};
    constexpr std::string_view MetalnessRoughnessTexture{"Metalness/Roughness Texture"};
    constexpr std::string_view Emissive{"Emissive"};
    constexpr std::string_view EmissiveTexture{"Emissive Texture"};
    constexpr std::string_view NormalMapTexture{"Normal Map Texture"};
}