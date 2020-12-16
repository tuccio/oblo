#pragma once

#include <filesystem>

namespace oblo
{
    struct sandbox_state;

    class scene_importer
    {
    public:
        bool import(sandbox_state& state, const std::filesystem::path& filename);
    };
}