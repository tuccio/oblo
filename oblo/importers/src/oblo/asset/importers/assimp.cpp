#include <oblo/asset/importers/assimp.hpp>

#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/processing/mesh_processing.hpp>
#include <oblo/core/buffered_array.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/thread/parallel_for.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace oblo::importers
{
    namespace
    {
        struct import_hierarchy
        {
            u32 nodeIndex;
        };

        struct import_mesh
        {
            u32 meshIndex;
            u32 nodeIndex;
        };

        struct import_material
        {
            u32 nodeIndex;
        };
    }

    struct assimp::impl
    {
        // Create an Importer per file_importer, for thread-safety
        // Supposedly this is expensive, so we might want to evaluate different options in the future
        Assimp::Importer importer;

        // The scene, owned by the importer
        const aiScene* scene{};

        uuid mainArtifactHint{};

        dynamic_array<import_artifact> artifacts;

        dynamic_array<import_mesh> importMeshes;
        dynamic_array<import_material> importMaterials;
        import_hierarchy importHierarchy;

        dynamic_array<string> sourceFiles;
    };

    assimp::assimp() = default;

    assimp::~assimp() = default;

    bool assimp::init(const import_config& config, import_preview& preview)
    {
        m_impl = allocate_unique<impl>();

        constexpr u32 postProcessFlags = aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_Triangulate;
        const aiScene* const scene = m_impl->importer.ReadFile(config.sourceFile.c_str(), postProcessFlags);

        if (!scene || !scene->mRootNode)
        {
            return false;
        }

        m_impl->sourceFiles.push_back(config.sourceFile);
        m_impl->scene = scene;

        string_builder nameBuilder;

        if (scene->HasMeshes())
        {
            m_impl->importMeshes.reserve(scene->mNumMeshes);

            for (u32 meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
            {
                auto* const aiMesh = scene->mMeshes[meshIndex];

                const u32 nodeIndex = preview.nodes.size32();

                m_impl->importMeshes.push_back({
                    .meshIndex = meshIndex,
                    .nodeIndex = nodeIndex,
                });

                nameBuilder.clear().append(aiMesh->mName.C_Str());

                if (nameBuilder.empty())
                {
                    nameBuilder.format("Mesh #{}", meshIndex);
                }

                preview.nodes.emplace_back(resource_type<mesh>, nameBuilder.as<string>());
            }
        }

        if (scene->HasMaterials())
        {
            m_impl->importMaterials.reserve(scene->mNumMaterials);

            for (u32 materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
            {
                auto* const aiMaterial = scene->mMaterials[materialIndex];

                const u32 nodeIndex = preview.nodes.size32();

                m_impl->importMaterials.push_back({
                    .nodeIndex = nodeIndex,
                });

                nameBuilder.clear().append(aiMaterial->GetName().C_Str());

                if (nameBuilder.empty())
                {
                    nameBuilder.format("Material #{}", materialIndex);
                }

                preview.nodes.emplace_back(resource_type<mesh>, nameBuilder.as<string>());
            }
        }

        {
            // Looks like there's only 1 hierarchy
            const u32 nodeIndex = preview.nodes.size32();

            m_impl->importHierarchy = {
                .nodeIndex = nodeIndex,
            };

            preview.nodes.emplace_back(resource_type<mesh>, "Hierarchy");
        }

        return true;
    }

    bool assimp::import(import_context ctx)
    {
        entity_hierarchy_serialization_context ehCtx;

        if (!ehCtx.init())
        {
            log::error("Failed to initialize entity hierarchy context");
            return false;
        }

        const aiScene* const scene = m_impl->scene;

        const std::span importNodeConfigs = ctx.get_import_node_configs();
        const std::span importNodes = ctx.get_import_nodes();

        const auto& hierarchyNodeConfig = importNodeConfigs[m_impl->importHierarchy.nodeIndex];

        // Filled by the parallel for, if the string is empty we assume it failed
        const u32 numMeshes = scene->mNumMeshes;
        dynamic_array<string_builder> meshOutputs;
        meshOutputs.resize(numMeshes);

        // We could avoid creating jobs for meshes that are disabled, but currently there's no way in the UI to disable
        // import of a node
        parallel_for(
            [this, scene, &importNodeConfigs, &ctx, &meshOutputs](job_range r)
            {
                buffered_array<mesh_attribute, 8> attributes;

                for (u32 meshIndex = r.begin; meshIndex != r.end; ++meshIndex)
                {
                    const auto& importMesh = m_impl->importMeshes[meshIndex];
                    const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

                    if (!meshNodeConfig.enabled)
                    {
                        continue;
                    }

                    // We triangulate, so we expect triangles
                    constexpr u32 indicesPerFace = 3;

                    mesh srcMesh;

                    aiMesh* const aiMesh = scene->mMeshes[importMesh.meshIndex];

                    const auto indexFormat =
                        aiMesh->mNumVertices > std::numeric_limits<u16>::max() ? data_format::u32 : data_format::u16;

                    if (aiMesh->HasPositions())
                    {
                        attributes.push_back({
                            .kind = attribute_kind::position,
                            .format = data_format::vec3,
                        });
                    }

                    if (aiMesh->HasFaces())
                    {
                        attributes.push_back({
                            .kind = attribute_kind::indices,
                            .format = indexFormat,
                        });
                    }

                    if (aiMesh->HasNormals())
                    {
                        attributes.push_back({
                            .kind = attribute_kind::normal,
                            .format = data_format::vec3,
                        });
                    }

                    if (aiMesh->HasTangentsAndBitangents())
                    {
                        attributes.push_back({
                            .kind = attribute_kind::tangent,
                            .format = data_format::vec3,
                        });

                        attributes.push_back({
                            .kind = attribute_kind::bitangent,
                            .format = data_format::vec3,
                        });
                    }

                    srcMesh.allocate(primitive_kind::triangle,
                        aiMesh->mNumVertices,
                        aiMesh->mNumFaces * indicesPerFace,
                        0,
                        attributes);

                    if (aiMesh->HasPositions())
                    {
                        const std::span positions = srcMesh.get_attribute<vec3>(attribute_kind::position);
                        std::memcpy(positions.data(), aiMesh->mVertices, positions.size_bytes());
                    }

                    if (aiMesh->HasNormals())
                    {
                        const std::span normals = srcMesh.get_attribute<vec3>(attribute_kind::normal);
                        std::memcpy(normals.data(), aiMesh->mNormals, normals.size_bytes());
                    }

                    if (aiMesh->HasTangentsAndBitangents())
                    {
                        const std::span tangents = srcMesh.get_attribute<vec3>(attribute_kind::tangent);
                        std::memcpy(tangents.data(), aiMesh->mTangents, tangents.size_bytes());

                        const std::span bitangents = srcMesh.get_attribute<vec3>(attribute_kind::bitangent);
                        std::memcpy(bitangents.data(), aiMesh->mBitangents, bitangents.size_bytes());
                    }

                    if (aiMesh->HasFaces())
                    {
                        auto fillIndices = [aiMesh]<typename T>(std::span<T> indices)
                        {
                            auto outIt = indices.begin();

                            for (const auto& face : std::span{aiMesh->mFaces, aiMesh->mNumFaces})
                            {
                                if (face.mNumIndices != indicesPerFace)
                                {
                                    continue;
                                }

                                *outIt++ = narrow_cast<u16>(face.mIndices[0]);
                                *outIt++ = narrow_cast<u16>(face.mIndices[1]);
                                *outIt++ = narrow_cast<u16>(face.mIndices[2]);
                            }
                        };

                        switch (indexFormat)
                        {
                        case data_format::u16:
                            fillIndices(srcMesh.get_attribute<u16>(attribute_kind::indices));
                            break;

                        case data_format::u32:
                            fillIndices(srcMesh.get_attribute<u32>(attribute_kind::indices));
                            break;

                        default:
                            unreachable();
                        }
                    }

                    mesh outMesh;

                    if (!mesh_processing::build_meshlets(srcMesh, outMesh))
                    {
                        log::error("Failed to build meshlets");
                        continue;
                    }

                    outMesh.update_aabb();

                    string_builder& outputPath = meshOutputs[meshIndex];

                    if (!save_mesh(outMesh, ctx.get_output_path(meshNodeConfig.id, outputPath, ".gltf")))
                    {
                        log::error("Failed to save mesh");

                        // Signals the import of the mesh failed
                        outputPath.clear();
                        continue;
                    }
                }
            },
            job_range{0, numMeshes},
            1);

        for (u32 meshIndex = 0; meshIndex < meshOutputs.size32(); ++meshIndex)
        {
            auto& outputPath = meshOutputs[meshIndex];

            const auto& importMesh = m_impl->importMeshes[meshIndex];
            const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

            m_impl->artifacts.push_back({
                .id = meshNodeConfig.id,
                .type = resource_type<mesh>,
                .name = importNodes[importMesh.nodeIndex].name,
                .path = outputPath.as<string>(),
            });
        }

        for (u32 materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            const auto& importMaterial = m_impl->importMaterials[materialIndex];
            const auto& materialNodeConfig = importNodeConfigs[importMaterial.nodeIndex];

            if (!materialNodeConfig.enabled)
            {
                continue;
            }

            material outMaterial;

            const aiMaterial* aiMaterial = scene->mMaterials[materialIndex];

            vec3 albedo = vec3::splat(1.f);
            vec3 emissiveFactor = vec3::splat(0.f);
            f32 emissiveMultiplier = 1.f;
            f32 metallic = 0.f;
            f32 roughness = .5f;
            f32 ior = 1.5f;

            resource_ref<texture> albedoTexture{};
            resource_ref<texture> metalnessRoughnessTexture{};
            resource_ref<texture> normalMapTexture{};
            resource_ref<texture> emissiveTexture{};

            if (aiColor4D color; aiGetMaterialColor(aiMaterial, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS)
            {
                albedo = {color.r, color.g, color.b};
            }

            if (aiColor4D color; aiGetMaterialColor(aiMaterial, AI_MATKEY_COLOR_EMISSIVE, &color) == AI_SUCCESS)
            {
                emissiveFactor = {color.r, color.g, color.b};
            }

            if (f32 factor; aiGetMaterialFloat(aiMaterial, AI_MATKEY_METALLIC_FACTOR, &factor) == AI_SUCCESS)
            {
                metallic = factor;
            }

            if (f32 factor; aiGetMaterialFloat(aiMaterial, AI_MATKEY_ROUGHNESS_FACTOR, &factor) == AI_SUCCESS)
            {
                roughness = factor;
            }

            if (f32 factor; aiGetMaterialFloat(aiMaterial, AI_MATKEY_EMISSIVE_INTENSITY, &factor) == AI_SUCCESS)
            {
                emissiveMultiplier = factor;
            }

            if (f32 factor; aiGetMaterialFloat(aiMaterial, AI_MATKEY_REFRACTI, &factor) == AI_SUCCESS)
            {
                ior = factor;
            }

            // TODO: If the material has specular, we need to convert to metallic

            outMaterial.set_property<material_type_tag::linear_color>(pbr::Albedo, albedo);

            outMaterial.set_property<material_type_tag::linear_color>(pbr::Emissive, emissiveFactor);
            outMaterial.set_property(pbr::EmissiveMultiplier, emissiveMultiplier);

            outMaterial.set_property(pbr::Metalness, metallic);
            outMaterial.set_property(pbr::Roughness, roughness);
            outMaterial.set_property(pbr::IndexOfRefraction, ior);

            outMaterial.set_property(pbr::AlbedoTexture, albedoTexture);
            outMaterial.set_property(pbr::MetalnessRoughnessTexture, metalnessRoughnessTexture);
            outMaterial.set_property(pbr::NormalMapTexture, normalMapTexture);
            outMaterial.set_property(pbr::EmissiveTexture, emissiveTexture);

            string_builder outputPath;
            ctx.get_output_path(materialNodeConfig.id, outputPath, ".omaterial");

            if (!outMaterial.save(outputPath))
            {
                log::error("Failed to save material to {}", outputPath);
                continue;
            }

            m_impl->artifacts.push_back({
                .id = materialNodeConfig.id,
                .type = resource_type<material>,
                .name = importNodes[importMaterial.nodeIndex].name,
                .path = outputPath.as<string>(),
            });
        }

        for (u32 hierarchyIndex = 0; hierarchyIndex < 1; ++hierarchyIndex)
        {
            if (!hierarchyNodeConfig.enabled)
            {
                continue;
            }

            entity_hierarchy h;

            if (!h.init(ehCtx.get_type_registry()))
            {
                log::error("Failed to initialize entity hierarchy");
                continue;
            }

            auto& reg = h.get_entity_registry();

            struct stack_info
            {
                ecs::entity parent;
                aiNode* node;
            };

            deque<stack_info> stack;

            stack.push_back({
                .parent = {},
                .node = scene->mRootNode,
            });

            while (!stack.empty())
            {
                const auto [parent, node] = stack.back();
                stack.pop_back();

                aiVector3D aiScale, aiPosition;
                aiQuaternion aiRotation;
                node->mTransformation.Decompose(aiScale, aiRotation, aiPosition);

                const vec3 scale = {.x = aiScale.x, .y = aiScale.y, .z = aiScale.z};
                const quaternion rotation{.x = aiRotation.x, .y = aiRotation.y, .z = aiRotation.z, .w = aiRotation.w};
                const vec3 translation = {.x = aiPosition.x, .y = aiPosition.y, .z = aiPosition.z};

                const auto e = ecs_utility::create_named_physical_entity(reg,
                    node->mName.C_Str(),
                    parent,
                    translation,
                    rotation,
                    scale);

                if (node->mNumMeshes >= 0)
                {
                    for (u32 meshIndex : std::span{node->mMeshes, node->mNumMeshes})
                    {
                        const auto& importMesh = m_impl->importMeshes[meshIndex];
                        const auto& meshNode = importNodes[importMesh.nodeIndex];

                        const auto m = ecs_utility::create_named_physical_entity<static_mesh_component>(reg,
                            meshNode.name,
                            e,
                            vec3::splat(0.f),
                            quaternion::identity(),
                            vec3::splat(1.f));

                        const u32 materialIndex = scene->mMeshes[meshIndex]->mMaterialIndex;

                        const auto& meshNodeConfig = importNodeConfigs[importMesh.nodeIndex];

                        auto& sm = reg.get<static_mesh_component>(m);
                        sm.mesh = resource_ref<mesh>{meshNodeConfig.id};
                        sm.material = resource_ref<material>{materialIndex < scene->mNumMaterials
                                ? importNodeConfigs[m_impl->importMaterials[materialIndex].nodeIndex].id
                                : uuid{}};
                    }
                }

                for (aiNode* child : std::span{node->mChildren, node->mNumChildren})
                {
                    stack.push_back({.parent = e, .node = child});
                }
            }

            string_builder outputPath;
            ctx.get_output_path(hierarchyNodeConfig.id, outputPath, ".ohierarchy");

            if (!h.save(outputPath, ehCtx))
            {
                log::error("Failed to save entity hierarchy");
                continue;
            }

            m_impl->artifacts.push_back({
                .id = hierarchyNodeConfig.id,
                .type = resource_type<oblo::entity_hierarchy>,
                .name = importNodes[m_impl->importHierarchy.nodeIndex].name,
                .path = outputPath.as<string>(),
            });

            m_impl->mainArtifactHint = hierarchyNodeConfig.id;
        }

        return true;
    }

    file_import_results assimp::get_results()
    {
        return {
            .artifacts = m_impl->artifacts,
            .sourceFiles = m_impl->sourceFiles,
            .mainArtifactHint = m_impl->mainArtifactHint,
        };
    }
}