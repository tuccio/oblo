#pragma once

#include <oblo/core/string/hashed_string_view.hpp>

namespace oblo
{
    /// @brief Can be used in reflection as a concept on components to mark components to be uploaded to GPU.
    struct gpu_component
    {
        /// @brief The name that will be used to bind the GPU buffer to shaders.
        hashed_string_view bufferName;
    };
}