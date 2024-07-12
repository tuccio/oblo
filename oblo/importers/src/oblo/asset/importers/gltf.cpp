#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/asset/importers/stb_image.hpp>
#include <oblo/asset/processing/mesh_processing.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/log.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/pbr_properties.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/thread/parallel_for.hpp>

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
        bool isOcclusionMetalnessRoughness;
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

        struct gltf_import_config
        {
            bool generateMeshlets{true};
        };

        vec3 get_vec3_or(const std::vector<double>& value, vec3 fallback)
        {
            if (value.size() >= 3)
            {
                return {f32(value[0]), f32(value[1]), f32(value[2])};
            }
            else
            {
                return fallback;
            }
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

        m_importMaterials.reserve(m_model.materials.size());

        for (u32 materialIndex = 0; materialIndex < m_model.materials.size(); ++materialIndex)
        {
            auto& gltfMaterial = m_model.materials[materialIndex];
            m_importMaterials.emplace_back(u32(preview.nodes.size()));
            preview.nodes.emplace_back(get_type_id<material>(), gltfMaterial.name);

            const auto metallicRoughness = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;

            if (metallicRoughness >= 0 && metallicRoughness == gltfMaterial.occlusionTexture.index)
            {
                m_importImages[metallicRoughness].isOcclusionMetalnessRoughness = true;
            }
        }

        return true;
    }

    bool gltf::import(const import_context& ctx)
    {
        gltf_import_config cfg{};

        if (const auto generateMeshlets = ctx.settings.find_child(ctx.settings.get_root(), "generateMeshlets");
            generateMeshlets != data_node::Invalid)
        {
            cfg.generateMeshlets = ctx.settings.read_bool(generateMeshlets).value_or(true);
        }

        dynamic_array<mesh_attribute> attributes;
        attributes.reserve(32);

        dynamic_array<gltf_accessor> sources;
        sources.reserve(32);

        dynamic_array<bool> usedBuffer;
        usedBuffer.resize(m_model.buffers.size());

        parallel_for(
            [&](job_range range)
            {
                for (u32 i = range.begin; i < range.end; ++i)
                {
                    auto& image = m_importImages[i];

                    if (!image.importer || !ctx.importNodesConfig[image.nodeIndex].enabled)
                    {
                        continue;
                    }

                    data_document settings;

                    if (image.isOcclusionMetalnessRoughness)
                    {
                        // When the texture is an ORM map, we drop the occlusion and swap roughness and metalness back
                        settings.init();

                        const u32 swizzle = settings.child_array(settings.get_root(), "swizzle");

                        settings.child_value(swizzle, {}, property_kind::u32, as_bytes(u32{2}));
                        settings.child_value(swizzle, {}, property_kind::u32, as_bytes(u32{1}));
                    }

                    const auto imageContext = import_context{
                        .registry = ctx.registry,
                        .nodes = ctx.nodes.subspan(image.nodeIndex, 1),
                        .importNodesConfig = ctx.importNodesConfig.subspan(image.nodeIndex, 1),
                        .importUuid = ctx.importUuid,
                        .settings = settings,
                    };

                    if (image.importer->import(imageContext))
                    {
                        OBLO_ASSERT(image.id.is_nil());
                        image.id = imageContext.importNodesConfig[0].id;
                    }
                }
            },
            job_range{0, u32(m_importImages.size())},
            1);

        for (auto& image : m_importImages)
        {
            if (!image.importer || !ctx.importNodesConfig[image.nodeIndex].enabled)
            {
                continue;
            }

            if (image.id.is_nil())
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

            set_texture(materialArtifact, pbr::AlbedoTexture, pbr.baseColorTexture.index);
            set_texture(materialArtifact, pbr::MetalnessRoughnessTexture, pbr.metallicRoughnessTexture.index);
            set_texture(materialArtifact, pbr::NormalMapTexture, gltfMaterial.normalTexture.index);
            set_texture(materialArtifact, pbr::EmissiveTexture, gltfMaterial.emissiveTexture.index);

            materialArtifact.set_property(pbr::Albedo, get_vec3_or(pbr.baseColorFactor, vec3::splat(1.f)));

            materialArtifact.set_property(pbr::Metalness, f32(pbr.metallicFactor));
            materialArtifact.set_property(pbr::Roughness, f32(pbr.roughnessFactor));

            materialArtifact.set_property(pbr::Emissive, get_vec3_or(gltfMaterial.emissiveFactor, vec3::splat(0.f)));

            f32 ior{1.5f};

            if (const auto it = gltfMaterial.extensions.find("KHR_materials_ior");
                it != gltfMaterial.extensions.end() && it->second.IsObject())
            {
                ior = f32(it->second.Get("ior").GetNumberAsDouble());
            }

            materialArtifact.set_property(pbr::IndexOfRefraction, ior);

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
            if (node.mesh >= 0 && usize(node.mesh) <= m_importModels.size())
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

                oblo::mesh srcMesh;

                if (!load_mesh(srcMesh,
                        m_model,
                        primitive,
                        attributes,
                        sources,
                        &usedBuffer,
                        mesh_post_process::generate_tanget_space))
                {
                    log::error("Failed to parse mesh");
                    continue;
                }

                if (model.applyTransform)
                {
                    const std::span positions = srcMesh.get_attribute<vec3>(attribute_kind::position);

                    for (auto& p : positions)
                    {
                        p = transform(model.rotation, p) * model.scale + model.translation;
                    }
                }

                oblo::mesh outMesh;

                if (cfg.generateMeshlets)
                {
                    if (!mesh_processing::build_meshlets(srcMesh, outMesh))
                    {
                        log::error("Failed to build meshlets");
                        continue;
                    }
                }
                else
                {
                    outMesh = std::move(srcMesh);
                }

                outMesh.update_aabb();

                modelAsset.meshes.emplace_back(meshNodeConfig.id);
                modelAsset.materials.emplace_back(
                    primitive.material >= 0 ? m_importMaterials[primitive.material].id : uuid{});

                m_artifacts.push_back({
                    .id = meshNodeConfig.id,
                    .data = any_asset{std::move(outMesh)},
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

    void gltf::set_texture(material& m, std::string_view propertyName, int textureIndex) const
    {
        if (const auto imageIndex = find_image_from_texture(m_model, textureIndex);
            imageIndex >= 0 && usize(imageIndex) < m_importImages.size() && !m_importImages[imageIndex].id.is_nil())
        {
            m.set_property(propertyName, resource_ref<texture>(m_importImages[imageIndex].id));
        }
    }
}