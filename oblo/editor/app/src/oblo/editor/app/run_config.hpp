#pragma once

#include <oblo/core/string/string.hpp>

namespace oblo::editor
{
    struct run_config
    {
        string appDir;
        string projectPath;
    };
}