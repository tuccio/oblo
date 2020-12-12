#pragma once

#include <assimp/Importer.hpp>
#include <filesystem>

namespace oblo
{
    struct sandbox_state;

    class scene_importer
    {
    public:
        bool import(sandbox_state& state, const std::filesystem::path& filename);

    private:
        Assimp::Importer m_importer;
    };
}