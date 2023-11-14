#pragma once

#include <oblo/asset/importer.hpp>

#include <tiny_gltf.h>

#include <filesystem>
#include <vector>

namespace oblo::importers
{
    class gltf final : public file_importer
    {
    public:
        gltf();
        gltf(const gltf&) = delete;
        gltf(gltf&&) noexcept = delete;
        gltf& operator=(const gltf&) = delete;
        gltf& operator=(gltf&&) noexcept = delete;
        ~gltf();

        bool init(const importer_config& config, import_preview& preview);
        bool import(const import_context& context);
        file_import_results get_results();

    private:
        struct import_model;
        struct import_mesh;
        struct import_image;

    private:
        tinygltf::Model m_model;
        tinygltf::TinyGLTF m_loader;
        std::vector<import_model> m_importModels;
        std::vector<import_mesh> m_importMeshes;
        std::vector<import_image> m_importImages;

        std::vector<import_artifact> m_artifacts;
        std::vector<std::filesystem::path> m_sourceFiles;
        uuid m_mainArtifactHint{};
    };
}