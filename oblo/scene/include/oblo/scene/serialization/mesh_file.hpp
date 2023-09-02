#pragma once

#include <filesystem>

namespace oblo::scene
{
    class mesh;

    bool save_mesh(const mesh& mesh, const std::filesystem::path& destination);
}