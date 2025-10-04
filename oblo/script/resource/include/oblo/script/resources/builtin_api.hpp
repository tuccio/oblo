#pragma once

#include <oblo/core/string/hashed_string_view.hpp>

namespace oblo::script_api
{
    namespace ecs
    {
        constexpr hashed_string_view get_property = "__ecs_get_property"_hsv;
        constexpr hashed_string_view set_property = "__ecs_set_property"_hsv;
    }

    constexpr hashed_string_view get_time = "__get_time"_hsv;

    constexpr hashed_string_view void_t = "void";
    constexpr hashed_string_view f32_t = "f32";
    constexpr hashed_string_view i32_t = "i32";
}