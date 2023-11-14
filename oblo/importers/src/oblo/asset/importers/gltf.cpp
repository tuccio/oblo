#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>

#include <format>

namespace oblo::importers
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

    struct gltf::import_image
    {
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

        m_loader.SetImageLoader(
            [](tinygltf::Image* image,
                const int imageIdx,
                std::string* err,
                std::string* warn,
                int reqWidth,
                int reqHeight,
                const unsigned char* bytes,
                int size,
                void* userdata)
            {
                // We skip the images for now
                (void) image;
                (void) imageIdx;
                (void) err;
                (void) warn;
                (void) reqWidth;
                (void) reqHeight;
                (void) bytes;
                (void) size;
                (void) userdata;
                return true;
            },
            this);

        if (sourceFileStr.ends_with(".glb"))
        {
            success = m_loader.LoadBinaryFromFile(&m_model, &errors, &warnings, sourceFileStr);
        }
        else if (sourceFileStr.ends_with(".gltf"))
        {
            success = m_loader.LoadASCIIFromFile(&m_model, &errors, &warnings, sourceFileStr);
        }
        else
        {
            // TODO: Report error
            return false;
        }

        if (!success)
        {
            log::error("Import of {} failed:\n{}", sourceFileStr, errors);
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

            preview.nodes.emplace_back(get_type_id<model>(), name);

            for (u32 primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex)
            {
                name.resize(prefixLen);
                std::format_to(std::back_insert_iterator(name), "#{}", meshIndex);

                m_importMeshes.push_back({
                    .mesh = meshIndex,
                    .primitive = primitiveIndex,
                    .nodeIndex = u32(preview.nodes.size()),
                });

                preview.nodes.emplace_back(get_type_id<mesh>(), name);
            }
        }

        return true;
    }

    bool gltf::import(const import_context& ctx)
    {
        std::vector<mesh_attribute> attributes;
        attributes.reserve(32);

        std::vector<gltf_accessor> sources;
        sources.reserve(32);

        std::vector<bool> usedBuffer(m_model.buffers.size());

        for (const auto& model : m_importModels)
        {
            oblo::model modelAsset;

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

                oblo::mesh meshAsset;

                if (!load_mesh(meshAsset, m_model, primitive, attributes, sources, &usedBuffer))
                {
                    continue;
                }

                modelAsset.meshes.emplace_back(meshNodeConfig.id);

                m_artifacts.push_back({
                    .id = meshNodeConfig.id,
                    .data = any_asset{std::move(meshAsset)},
                    .name = ctx.preview->nodes[mesh.nodeIndex].name,
                });
            }

            m_artifacts.push_back({
                .id = modelNodeConfig.id,
                .data = any_asset{std::move(modelAsset)},
                .name = ctx.preview->nodes[model.nodeIndex].name,
            });

            if (m_importModels.size() == 1)
            {
                m_mainArtifactHint = modelNodeConfig.id;
            }
        }

        const auto sourceFile = ctx.importer->get_config().sourceFile;
        const auto sourceFileDir = sourceFile.parent_path();

        m_sourceFiles.reserve(1 + m_model.buffers.size());
        m_sourceFiles.emplace_back(sourceFile);

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
                    m_sourceFiles.emplace_back(bufferPath);
                }
            }
        }

        return true;
    }

    file_import_results gltf::get_results()
    {
        return {
            .artifacts = m_artifacts,
            .sourceFiles = m_sourceFiles,
            .mainArtifactHint = m_mainArtifactHint,
        };
    }
}