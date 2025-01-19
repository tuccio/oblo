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
            .defaultValue = property_value_wrapper{true},
        };
    };

    template <>
    struct option_traits<"r.shaders.enablePrintf">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "02a9a58d-636c-4dc8-9990-e580b50e031b"_uuid,
            .name = "Enable shader printf",
            .category = "Graphics/Shaders",
            .defaultValue = property_value_wrapper{false},
        };
    };

    template <>
    struct option_traits<"r.shaders.enableSpirvCache">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "37b235f2-0141-4381-ab81-aee8ba9f6fde"_uuid,
            .name = "Enable SPIR-V cache",
            .category = "Graphics/Shaders",
            .defaultValue = property_value_wrapper{true},
        };
    };

    template <>
    struct option_traits<"r.shaders.emitLineDirectives">
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "73506162-fc22-4148-a7df-ee30c4726c95"_uuid,
            .name = "Emit line directives on shaders",
            .category = "Graphics/Shaders",
            .defaultValue = property_value_wrapper{true},
        };
    };

    struct global_shader_options_proxy
    {
        option_proxy<"r.shaders.preferGlslang"> preferGlslang;
        option_proxy<"r.shaders.optimizeShaders"> optimizeShaders;
        option_proxy<"r.shaders.emitDebugInfo"> emitDebugInfo;
        option_proxy<"r.shaders.enablePrintf"> enablePrintf;
        option_proxy<"r.shaders.enableSpirvCache"> enableSpirvCache;
        option_proxy<"r.shaders.emitLineDirectives"> emitLineDirectives;
    };

    struct global_shader_options
    {
        bool preferGlslang;
        bool optimizeShaders;
        bool emitDebugInfo;
        bool enablePrintf;
        bool enableSpirvCache;
        bool emitLineDirectives;
    };
}