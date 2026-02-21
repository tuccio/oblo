#pragma once

#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>

#include <span>

namespace oblo
{
    struct compute_pass_initializer
    {
        string_view name;
        string_view shaderSourcePath;
    };

    struct compute_pipeline_initializer
    {
        std::span<const hashed_string_view> defines;
    };
}