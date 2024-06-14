#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/asset/importers/stb_image.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/pbr_properties.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>

#include <format>

namespace oblo::importers
{
    struct gltf::import_model
    {
        u32 mesh;
        u32 nodeIndex;
        u32 primitiveBegin;

        bool applyTransform;

        vec3 translation;
        quaternion rotation;
        vec3 scale;
    };

    struct gltf::import_mesh
    {
        u32 mesh;
        u32 primitive;
        u32 nodeIndex;
    };

    struct gltf::import_material
    {
        u32 nodeIndex;
        uuid id;
    };

    struct gltf::import_image
    {
        u32 nodeIndex;
        std::unique_ptr<file_importer> importer;
        // This is only set after importing is done
        uuid id;
    };

    namespace
    {
        int find_image_from_texture(const tinygltf::Model& model, int textureIndex)
        {
            if (textureIndex < 0)
            {
                return textureIndex;
            }

            return usize(textureIndex) < model.textures.size() ? model.textures[textureIndex].source : -1;
        }
    }

    gltf::gltf() = default;

    gltf::~gltf() = default;

    bool gltf::init(const importer_config& config, import_preview& preview)
    {
        // Seems like TinyGLTF wants std::string
        const auto sourceFileStr = config.sourceFile.string();

        m_sourceFiles.reserve(1 + m_model.buffers.size() + m_model.images.size());
        m_sourceFiles.emplace_back(config.sourceFile);

        m_sourceFileDir = m_sourceFiles[0].parent_path();

        std::string errors;
        std::string warnings;

        bool success;

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

        m_importMaterials.reserve(m_model.materials.size());

        for (u32 materialIndex = 0; materialIndex < m_model.materials.size(); ++materialIndex)
        {
            auto& gltfMaterial = m_model.materials[materialIndex];
            m_importMaterials.emplace_back(u32(preview.nodes.size()));
            preview.nodes.emplace_back(get_type_id<material>(), gltfMaterial.name);
        }

        import_preview imagePreview;

        m_importImages.reserve(m_model.images.size());

        for (u32 imageIndex = 0; imageIndex < m_model.images.size(); ++imageIndex)
        {
            auto& gltfImage = m_model.images[imageIndex];

            auto& importImage = m_importImages.emplace_back();

            if (gltfImage.uri.empty())
            {
                log::warn("A texture was skipped because URI is not set, maybe it's embedded in the GLTF but this is "
                          "not supported currently.");

                continue;
            }

            if (!tinygltf::URIDecode(gltfImage.uri, &name, nullptr))
            {
                log::error("Failed to decode URI {}", gltfImage.uri);
                continue;
            }

            const u32 nodeIndex = u32(preview.nodes.size());
            preview.nodes.emplace_back(get_type_id<texture>(), name);

            // TODO: Look for the best registered image importer instead
            auto importer = std::make_unique<stb_image>();

            imagePreview.nodes.clear();

            const bool ok = importer->init(
                {
                    .registry = config.registry,
                    .sourceFile = m_sourceFileDir / name,
                },
                imagePreview);

            if (!ok || imagePreview.nodes.size() != 1 || imagePreview.nodes[0].type != get_type_id<texture>())
            {
                log::warn("Texture {} skipped due to incompatible image importer", gltfImage.uri);
                continue;
            }

            importImage.nodeIndex = nodeIndex;
            importImage.importer = std::move(importer);
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

        // TODO: Parallelize texture importing
        for (auto& image : m_importImages)
        {
            if (!image.importer || !ctx.importNodesConfig[image.nodeIndex].enabled)
            {
                continue;
            }

            const auto imageContext = import_context{
                .registry = ctx.registry,
                .nodes = ctx.nodes.subspan(image.nodeIndex, 1),
                .importNodesConfig = ctx.importNodesConfig.subspan(image.nodeIndex, 1),
                .importUuid = ctx.importUuid,
            };

            if (!image.importer->import(imageContext))
            {
                log::error("Failed to import image {}", ctx.nodes[image.nodeIndex].name);
                continue;
            }
            else
            {
                const auto results = image.importer->get_results();
                m_sourceFiles.insert(m_sourceFiles.end(), results.sourceFiles.begin(), results.sourceFiles.end());

                OBLO_ASSERT(results.artifacts.size() == 1, "Not sure how this would be more than atm");

                for (auto& artifact : results.artifacts)
                {
                    m_artifacts.emplace_back(std::move(artifact));
                }

                image.id = imageContext.importNodesConfig[0].id;
            }
        }

        for (auto& material : m_importMaterials)
        {
            const auto& nodeConfig = ctx.importNodesConfig[material.nodeIndex];

            if (!nodeConfig.enabled)
            {
                continue;
            }

            oblo::material materialArtifact;

            const auto materialIndex = &material - m_importMaterials.data();
            auto& gltfMaterial = m_model.materials[materialIndex];

            auto& pbr = gltfMaterial.pbrMetallicRoughness;

            vec3 albedo;

            if (pbr.baseColorFactor.size() >= 3)
            {
                albedo = {f32(pbr.baseColorFactor[0]), f32(pbr.baseColorFactor[1]), f32(pbr.baseColorFactor[2])};
            }
            else
            {
                albedo = vec3::splat(1);
            }

            materialArtifact.set_property(pbr::Albedo, albedo);

            if (const auto imageIndex = find_image_from_texture(m_model, pbr.baseColorTexture.index);
                imageIndex >= 0 && usize(imageIndex) < m_importImages.size() && !m_importImages[imageIndex].id.is_nil())
            {
                materialArtifact.set_property(pbr::AlbedoTexture, resource_ref<texture>(m_importImages[imageIndex].id));
            }

            materialArtifact.set_property(pbr::Metalness, f32(pbr.metallicFactor));
            materialArtifact.set_property(pbr::Roughness, f32(pbr.roughnessFactor));

            if (const auto imageIndex = find_image_from_texture(m_model, pbr.metallicRoughnessTexture.index);
                imageIndex >= 0 && usize(imageIndex) < m_importImages.size() && !m_importImages[imageIndex].id.is_nil())
            {
                materialArtifact.set_property(pbr::MetalnessRoughnessTexture,
                    resource_ref<texture>(m_importImages[imageIndex].id));
            }

            const auto& name = ctx.nodes[material.nodeIndex].name;

            m_artifacts.push_back({
                .id = nodeConfig.id,
                .data = any_asset{std::move(materialArtifact)},
                .name = name.empty() ? std::format("Material#{}", materialIndex) : name,
            });

            material.id = nodeConfig.id;
        }

        for (auto& node : m_model.nodes)
        {
            if (node.mesh >= 0 && node.mesh <= m_importModels.size())
            {
                vec3 translation = vec3::splat(0.f);
                quaternion rotation = quaternion::identity();
                vec3 scale = vec3::splat(1.f);

                if (node.translation.size() == 3)
                {
                    translation = {f32(node.translation[0]), f32(node.translation[1]), f32(node.translation[2])};
                }

                if (node.scale.size() == 3)
                {
                    scale = {f32(node.scale[0]), f32(node.scale[1]), f32(node.scale[2])};
                }

                if (node.rotation.size() == 4)
                {
                    rotation = {
                        f32(node.rotation[0]),
                        f32(node.rotation[1]),
                        f32(node.rotation[2]),
                        f32(node.rotation[3]),
                    };
                }

                auto& model = m_importModels[node.mesh];

                model.applyTransform = true;
                model.translation = translation;
                model.rotation = rotation;
                model.scale = scale;
            }
        }

        for (const auto& model : m_importModels)
        {
            const auto& modelNodeConfig = ctx.importNodesConfig[model.nodeIndex];

            if (!modelNodeConfig.enabled)
            {
                continue;
            }

            const auto& gltfMesh = m_model.meshes[model.mesh];

            oblo::model modelAsset;

            const auto numPrimitives = gltfMesh.primitives.size();
            modelAsset.meshes.reserve(numPrimitives);
            modelAsset.materials.reserve(numPrimitives);

            for (u32 meshIndex = model.primitiveBegin; meshIndex < model.primitiveBegin + numPrimitives; ++meshIndex)
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

                if (model.applyTransform)
                {
                    const std::span positions = meshAsset.get_attribute<vec3>(attribute_kind::position);

                    for (auto& p : positions)
                    {
                        p = transform(model.rotation, p) * model.scale + model.translation;
                    }
                }

                meshAsset.update_aabb();

                modelAsset.meshes.emplace_back(meshNodeConfig.id);
                modelAsset.materials.emplace_back(
                    primitive.material >= 0 ? m_importMaterials[primitive.material].id : uuid{});

                m_artifacts.push_back({
                    .id = meshNodeConfig.id,
                    .data = any_asset{std::move(meshAsset)},
                    .name = ctx.nodes[mesh.nodeIndex].name,
                });
            }

            m_artifacts.push_back({
                .id = modelNodeConfig.id,
                .data = any_asset{std::move(modelAsset)},
                .name = ctx.nodes[model.nodeIndex].name,
            });

            if (m_importModels.size() == 1)
            {
                m_mainArtifactHint = modelNodeConfig.id;
            }
        }

        for (usize i = 0; i < usedBuffer.size(); ++i)
        {
            if (usedBuffer[i])
            {
                auto& buffer = m_model.buffers[i];

                if (buffer.uri.empty() || buffer.uri.starts_with("data:"))
                {
                    continue;
                }

                const auto bufferPath = m_sourceFileDir / buffer.uri;

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