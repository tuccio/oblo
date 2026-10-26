#pragma once

#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>

#include <oblo/core/span.hpp>

namespace oblo
{
    struct compute_pass_initializer
    {
        string_view name;
        string_view shaderSourcePath;
    };

    struct compute_pipeline_initializer
    {
        span<const hashed_string_view> defines;
    };
}