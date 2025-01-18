#pragma once

#include <oblo/core/types.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>

namespace oblo
{
    template <>
    struct option_traits<"r.shaders.preferGlslang">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "e900ccd9-500d-4115-ac10-e9ea9e1e05b9"_uuid,
            .name = "Prefer glslang over glslc",
            .category = "Graphics/Shaders",
            .defaultValue = property_value_wrapper{false},
        };
    };

    template <>
    struct option_traits<"r.shaders.optimizeShaders">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "c2a69090-7969-4ca4-9e89-c29123b8118d"_uuid,
            .name = "Optimize shaders",
            .category = "Graphics/Shaders",
            .defaultValue = property_value_wrapper{false},
        };
    };

    template <>
    struct option_traits<"r.shaders.emitDebugInfo">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "be64e526-68dd-440d-8b8d-e52ecc5f6a89"_uuid,
            .name = "Emit shader debug info",
            .category = "Graphics/Shaders",
            .defaultValue = property_value_wrapper{false},
        };
    };

    struct global_shader_options_proxy
    {
        option_proxy<"r.shaders.preferGlslang"> preferGlslang;
        option_proxy<"r.shaders.optimizeShaders"> optimizeShaders;
        option_proxy<"r.shaders.emitDebugInfo"> emitDebugInfo;
    };

    struct global_shader_options
    {
        bool preferGlslang;
        bool optimizeShaders;
        bool emitDebugInfo;
    };
}