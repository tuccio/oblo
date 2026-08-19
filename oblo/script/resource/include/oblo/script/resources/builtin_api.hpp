#pragma once

#include <oblo/core/string/hashed_string_view.hpp>

namespace oblo::script_api
{
    namespace ecs
    {
        constexpr hashed_string_view get_property_f32 = "__ecs_get_property_f32"_hsv;
        constexpr hashed_string_view set_property_f32 = "__ecs_set_property_f32"_hsv;

        constexpr hashed_string_view get_property_vec3 = "__ecs_get_property_vec3"_hsv;
        constexpr hashed_string_view set_property_vec3 = "__ecs_set_property_vec3"_hsv;
    }

    constexpr hashed_string_view get_time = "__get_time"_hsv;

    constexpr hashed_string_view invoke_reflected_function_void = "__invoke_reflected_function_void"_hsv;
    constexpr hashed_string_view invoke_reflected_function_i32 = "__invoke_reflected_function_i32"_hsv;
    constexpr hashed_string_view invoke_reflected_function_f32 = "__invoke_reflected_function_f32"_hsv;
    constexpr hashed_string_view invoke_reflected_function_vec3 = "__invoke_reflected_function_vec3"_hsv;
    constexpr hashed_string_view invoke_reflected_function_bool = "__invoke_reflected_function_bool"_hsv;

    constexpr hashed_string_view cosine_f32 = "__intrin_cos_f32"_hsv;
    constexpr hashed_string_view cosine_vec3 = "__intrin_cos_vec3"_hsv;
    constexpr hashed_string_view sine_f32 = "__intrin_sin_f32"_hsv;
    constexpr hashed_string_view sine_vec3 = "__intrin_sin_vec3"_hsv;

    constexpr hashed_string_view void_t = "void";
    constexpr hashed_string_view bool_t = "bool";
    constexpr hashed_string_view f32_t = "f32";
    constexpr hashed_string_view i32_t = "i32";
    constexpr hashed_string_view vec3_t = "vec3";
    constexpr hashed_string_view string_t = "cstring";

    constexpr hashed_string_view u32_t = "u32";
    constexpr hashed_string_view const_void_ptr_t = "const void*";
    constexpr hashed_string_view void_const_ptr_const_ptr_t = "const void* const*";
}