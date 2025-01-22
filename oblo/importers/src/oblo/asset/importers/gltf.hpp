#pragma once

#include <oblo/asset/import/importer.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string.hpp>

#include <tiny_gltf.h>

namespace oblo
{
    class material;
}

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
        bool import(import_context context);
        file_import_results get_results();

    private:
        struct import_model;
        struct import_mesh;
        struct import_material;
        struct import_image;

    private:
        void set_texture(material& m, hashed_string_view propertyName, int textureIndex) const;

    private:
        tinygltf::Model m_model;
        tinygltf::TinyGLTF m_loader;
        dynamic_array<import_model> m_importModels;
        dynamic_array<import_mesh> m_importMeshes;
        dynamic_array<import_material> m_importMaterials;
        dynamic_array<import_image> m_importImages;

        dynamic_array<import_artifact> m_artifacts;
        dynamic_array<string> m_sourceFiles;
        string m_sourceFileDir;
        uuid m_mainArtifactHint{};
    };
}