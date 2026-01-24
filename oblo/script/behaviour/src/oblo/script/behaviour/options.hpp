#pragma once

#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>

namespace oblo
{
    namespace script_options
    {
        constexpr fixed_string use_native_runtime = "g.script.runtime.useNative";
    }

    template <>
    struct option_traits<script_options::use_native_runtime>
    {
        using type = bool;

        static constexpr option_descriptor descriptor{
            .kind = property_kind::boolean,
            .id = "1edc81b4-564f-411b-836b-6f2c624e2b62"_uuid,
            .name = "Use native script runtime when possible",
            .category = "Script/Runtime",
            .defaultValue = property_value_wrapper{true},
        };
    };

    struct script_behaviour_options
    {
        option_proxy<script_options::use_native_runtime> useNativeRuntime;
    };
}