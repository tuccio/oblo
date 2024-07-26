#pragma once

#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_interner.hpp>

#include <span>

namespace oblo::vk
{
    struct compute_pass_initializer
    {
        string name;
        string shaderSourcePath;
    };

    struct compute_pipeline_initializer
    {
        std::span<const h32<string>> defines;
    };
}