#pragma once

#include <oblo/core/string_interner.hpp>

#include <filesystem>
#include <span>
#include <string>

namespace oblo::vk
{
    struct compute_pass_initializer
    {
        std::string name;
        std::filesystem::path shaderSourcePath;
    };

    struct compute_pipeline_initializer
    {
        std::span<const h32<string>> defines;
    };
}