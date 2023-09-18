#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>

#include <format>

namespace oblo::asset::importers
{
    struct gltf::import_model
    {
        u32 mesh;
        u32 nodeIndex;
        u32 primitiveBegin;
    };

    struct gltf::import_mesh
    {
        u32 mesh;
        u32 primitive;
        u32 nodeIndex;
    };

    gltf::gltf() = default;

    gltf::~gltf() = default;

    bool gltf::init(const importer_config& config, import_preview& preview)
    {
        // Seems like TinyGLTF wants std::string
        const auto sourceFileStr = config.sourceFile.string();

        std::string errors;
        std::string warnings;

        bool success;

        if (sourceFileStr.ends_with(".glb"))
        {
            success = m_loader.LoadBinaryFromFile(&m_model, &errors, &warnings, config.sourceFile.string());
        }
        else if (sourceFileStr.ends_with(".gltf"))
        {
            success = m_loader.LoadASCIIFromFile(&m_model, &errors, &warnings, config.sourceFile.string());
        }
        else
        {
            // TODO: Report error
            return false;
        }

        if (!success)
        {
            return false;
        }

        std::string name;

        for (u32 meshIndex = 0; meshIndex < m_model.meshes.size(); ++meshIndex)
        {
            const auto& gltfMesh = m_model.meshes[meshIndex];

            name.assign(gltfMesh.name);
            const auto prefixLen = name.size();

            m_importModels.push_back({
                .mesh = meshIndex,
                .nodeIndex = u32(preview.nodes.size()),
                .primitiveBegin = u32(m_importMeshes.size()),
            });

            preview.nodes.emplace_back(get_type_id<scene::model>(), name);

            for (u32 primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex)
            {
                name.resize(prefixLen);
                std::format_to(std::back_insert_iterator(name), "#{}", meshIndex);

                m_importMeshes.push_back({
                    .mesh = meshIndex,
                    .primitive = primitiveIndex,
                    .nodeIndex = u32(preview.nodes.size()),
                });

                preview.nodes.emplace_back(get_type_id<scene::mesh>(), name);
            }
        }

        return true;
    }

    bool gltf::import(const import_context& ctx)
    {
        std::vector<scene::mesh_attribute> attributes;
        attributes.reserve(32);

        std::vector<scene::gltf_accessor> sources;
        sources.reserve(32);

        std::vector<import_artifact> meshArtifacts;
        meshArtifacts.reserve(32);

        std::vector<bool> usedBuffer(m_model.buffers.size());

        for (const auto& model : m_importModels)
        {
            scene::model modelAsset;
            meshArtifacts.clear();

            const auto& modelNodeConfig = ctx.importNodesConfig[model.nodeIndex];

            if (!modelNodeConfig.enabled)
            {
                continue;
            }

            const auto& gltfMesh = m_model.meshes[model.mesh];

            for (u32 meshIndex = model.primitiveBegin; meshIndex < model.primitiveBegin + gltfMesh.primitives.size();
                 ++meshIndex)
            {
                const auto& mesh = m_importMeshes[meshIndex];
                const auto& meshNodeConfig = ctx.importNodesConfig[mesh.nodeIndex];

                if (!meshNodeConfig.enabled)
                {
                    continue;
                }

                const auto& primitive = m_model.meshes[mesh.mesh].primitives[mesh.primitive];

                scene::mesh meshAsset;

                if (!scene::load_mesh(meshAsset, m_model, primitive, attributes, sources, &usedBuffer))
                {
                    continue;
                }

                modelAsset.meshes.emplace_back(meshNodeConfig.id);

                auto& meshArtifact = meshArtifacts.emplace_back();
                meshArtifact.id = meshNodeConfig.id;
                meshArtifact.data = any_asset{std::move(meshAsset)};
                meshArtifact.name = ctx.preview->nodes[mesh.nodeIndex].name;
            }

            ctx.importer->add_asset(
                {
                    .id = modelNodeConfig.id,
                    .data = any_asset{std::move(modelAsset)},
                    .name = ctx.preview->nodes[model.nodeIndex].name,
                },
                meshArtifacts);
        }

        std::vector<std::filesystem::path> sourceFiles;
        sourceFiles.reserve(m_model.buffers.size());

        const auto sourceFileDir = ctx.importer->get_config().sourceFile.parent_path();

        for (usize i = 0; i < usedBuffer.size(); ++i)
        {
            if (usedBuffer[i])
            {
                auto& buffer = m_model.buffers[i];

                if (buffer.uri.empty() || buffer.uri.starts_with("data:"))
                {
                    continue;
                }

                const auto bufferPath = sourceFileDir / buffer.uri;

                if (std::error_code ec; std::filesystem::exists(bufferPath, ec) && !ec)
                {
                    sourceFiles.emplace_back(bufferPath);
                }
            }
        }

        ctx.importer->add_source_files(sourceFiles);

        return true;
    }
}