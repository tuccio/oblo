#pragma once

#include <oblo/asset/importer.hpp>

#include <tiny_gltf.h>

namespace oblo::asset::importers
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

        void init(const importer_config& config, import_preview& preview);
        bool import(const import_context& context);

    private:
        struct import_model;
        struct import_mesh;

    private:
        tinygltf::Model m_model;
        tinygltf::TinyGLTF m_loader;
        std::vector<import_model> m_importModels;
        std::vector<import_mesh> m_importMeshes;
    };
}