#pragma once

#include <oblo/core/string/fixed_string.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_value_wrapper.hpp>

namespace oblo
{
    template <fixed_string>
    struct option_traits;

    struct option_descriptor
    {
        property_kind kind;
        uuid id;
        string_view name;
        string_view category;
        property_value_wrapper defaultValue;
        property_value_wrapper minValue;
        property_value_wrapper maxValue;
    };
}