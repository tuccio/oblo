#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/importers/stb_image.hpp>
#include <oblo/asset/processing/mesh_processing.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/scene/serialization/model_file.hpp>
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
        usize subImportIndex;
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

    bool gltf::init(const import_config& config, import_preview& preview)
    {
        // Seems like TinyGLTF wants std::string
        const auto sourceFileStr = config.sourceFile.as<std::string>();

        m_sourceFiles.reserve(1 + m_model.buffers.size() + m_model.images.size());
        m_sourceFiles.emplace_back(config.sourceFile);

        m_sourceFileDir = filesystem::parent_path(m_sourceFiles[0]).as<string>();

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

        string_builder name;

        for (u32 meshIndex = 0; meshIndex < m_model.meshes.size(); ++meshIndex)
        {
            const auto& gltfMesh = m_model.meshes[meshIndex];

            name.clear().append(gltfMesh.name);

            m_importModels.push_back({
                .mesh = meshIndex,
                .nodeIndex = u32(preview.nodes.size()),
                .primitiveBegin = u32(m_importMeshes.size()),
            });

            preview.nodes.emplace_back(resource_type<model>, name.as<string>());

            for (u32 primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex)
            {
                name.clear().append(gltfMesh.name).format("#{}", meshIndex);

                m_importMeshes.push_back({
                    .mesh = meshIndex,
                    .primitive = primitiveIndex,
                    .nodeIndex = u32(preview.nodes.size()),
                });

                preview.nodes.emplace_back(resource_type<mesh>, name.as<string>());
            }
        }

        m_importImages.reserve(m_model.images.size());

        std::string stdStringBuf;

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

            if (!tinygltf::URIDecode(gltfImage.uri, &stdStringBuf, nullptr))
            {
                log::error("Failed to decode URI {}", gltfImage.uri);
                continue;
            }

            importImage.subImportIndex = preview.children.size();
            auto& subImport = preview.children.emplace_back();

            name.clear().append(stdStringBuf.c_str());
            preview.nodes.emplace_back(resource_type<texture>, name.as<string>());

            name.clear().append(m_sourceFileDir).append_path(stdStringBuf.c_str());
            subImport.sourceFile = name.as<string>();
        }

        m_importMaterials.reserve(m_model.materials.size());

        for (u32 materialIndex = 0; materialIndex < m_model.materials.size(); ++materialIndex)
        {
            auto& gltfMaterial = m_model.materials[materialIndex];
            m_importMaterials.emplace_back(u32(preview.nodes.size()));
            preview.nodes.emplace_back(resource_type<material>, string{gltfMaterial.name.c_str()});

            const auto metallicRoughness = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;

            if (metallicRoughness >= 0 && metallicRoughness == gltfMaterial.occlusionTexture.index &&
                usize(metallicRoughness) < m_importImages.size())
            {
                auto& subImport = preview.children[m_importImages[metallicRoughness].subImportIndex];
                auto& settings = subImport.settings;

                // When the texture is an ORM map, we drop the occlusion and swap roughness and metalness back
                settings.init();

                const u32 swizzle = settings.child_array(settings.get_root(), "swizzle");

                settings.child_value(swizzle, {}, property_kind::u32, as_bytes(u32{2}));
                settings.child_value(swizzle, {}, property_kind::u32, as_bytes(u32{1}));
            }
        }

        return true;
    }

    bool gltf::import(import_context ctx)
    {
        gltf_import_config cfg{};

        const auto& settings = ctx.get_settings();

        if (const auto generateMeshlets = settings.find_child(settings.get_root(), "generateMeshlets");
            generateMeshlets != data_node::Invalid)
        {
            cfg.generateMeshlets = settings.read_bool(generateMeshlets).value_or(true);
        }

        dynamic_array<mesh_attribute> attributes;
        attributes.reserve(32);

        dynamic_array<gltf_accessor> sources;
        sources.reserve(32);

        dynamic_array<bool> usedBuffer;
        usedBuffer.resize(m_model.buffers.size());

        const std::span importNodeConfigs = ctx.get_import_node_configs();
        const std::span importNodes = ctx.get_import_nodes();

        for (usize i = 0; i < m_importImages.size(); ++i)
        {
            const std::span childNodes = ctx.get_child_import_nodes(i);
            const std::span childNodeConfigs = ctx.get_child_import_node_configs(i);

            if (childNodes.size() == 1 && childNodeConfigs[0].enabled &&
                childNodes[0].artifactType == resource_type<texture>)
            {
                auto& image = m_importImages[i];
                image.id = childNodeConfigs[0].id;
            }
        }

        for (auto& material : m_importMaterials)
        {
            const auto& nodeConfig = importNodeConfigs[material.nodeIndex];

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

            materialArtifact.set_property<material_type_tag::linear_color>(pbr::Albedo,
                get_vec3_or(pbr.baseColorFactor, vec3::splat(1.f)));

            materialArtifact.set_property(pbr::Metalness, f32(pbr.metallicFactor));
            materialArtifact.set_property(pbr::Roughness, f32(pbr.roughnessFactor));

            {
                auto emissiveFactor = get_vec3_or(gltfMaterial.emissiveFactor, vec3::splat(0.f));
                const auto [r, g, b] = emissiveFactor;
                const auto highest = max(r, g, b);

                f32 emissiveMultiplier = 1.f;

                if (highest > 1.f)
                {
                    emissiveFactor = emissiveFactor / highest;
                    emissiveMultiplier = highest;
                }

                materialArtifact.set_property<material_type_tag::linear_color>(pbr::Emissive, emissiveFactor);
                materialArtifact.set_property(pbr::EmissiveMultiplier, emissiveMultiplier);
            }

            f32 ior{1.5f};

            if (const auto it = gltfMaterial.extensions.find("KHR_materials_ior");
                it != gltfMaterial.extensions.end() && it->second.IsObject())
            {
                ior = f32(it->second.Get("ior").GetNumberAsDouble());
            }

            materialArtifact.set_property(pbr::IndexOfRefraction, ior);

            string name = importNodes[material.nodeIndex].name;

            string_builder buffer;

            if (name.empty())
            {
                buffer.clear().format("Material#{}", materialIndex);
                name = buffer.as<string>();
            }

            if (const auto path = ctx.get_output_path(nodeConfig.id, buffer); !materialArtifact.save(path))
            {
                log::error("Failed to save material to {}", path);
                continue;
            }

            m_artifacts.push_back({
                .id = nodeConfig.id,
                .type = resource_type<oblo::material>,
                .name = std::move(name),
                .path = buffer.as<string>(),
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
            const auto& modelNodeConfig = importNodeConfigs[model.nodeIndex];

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
                const auto& meshNodeConfig = importNodeConfigs[mesh.nodeIndex];

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

                string_builder outputPath;

                if (!save_mesh(outMesh, ctx.get_output_path(meshNodeConfig.id, outputPath)))
                {
                    log::error("Failed to save mesh");
                    continue;
                }

                m_artifacts.push_back({
                    .id = meshNodeConfig.id,
                    .type = resource_type<oblo::mesh>,
                    .name = importNodes[mesh.nodeIndex].name,
                    .path = outputPath.as<string>(),
                });
            }

            string_builder outputPath;

            if (!save_model_json(modelAsset, ctx.get_output_path(modelNodeConfig.id, outputPath)))
            {
                log::error("Failed to save mesh");
                continue;
            }

            m_artifacts.push_back({
                .id = modelNodeConfig.id,
                .type = resource_type<oblo::model>,
                .name = importNodes[model.nodeIndex].name,
                .path = outputPath.as<string>(),
            });

            if (m_importModels.size() == 1)
            {
                m_mainArtifactHint = modelNodeConfig.id;
            }
        }

        string_builder bufferPathBuilder;

        for (usize i = 0; i < usedBuffer.size(); ++i)
        {
            if (usedBuffer[i])
            {
                auto& buffer = m_model.buffers[i];

                if (buffer.uri.empty() || buffer.uri.starts_with("data:"))
                {
                    continue;
                }

                bufferPathBuilder.clear().append(m_sourceFileDir).append_path(buffer.uri);

                if (filesystem::exists(bufferPathBuilder).value_or(false))
                {
                    m_sourceFiles.emplace_back(bufferPathBuilder.as<string>());
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

    void gltf::set_texture(material& m, hashed_string_view propertyName, int textureIndex) const
    {
        if (const auto imageIndex = find_image_from_texture(m_model, textureIndex);
            imageIndex >= 0 && usize(imageIndex) < m_importImages.size() && !m_importImages[imageIndex].id.is_nil())
        {
            m.set_property(propertyName, resource_ref<texture>(m_importImages[imageIndex].id));
        }
    }
}