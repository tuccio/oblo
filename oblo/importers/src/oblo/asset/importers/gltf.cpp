#include <oblo/asset/importers/gltf.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/scene/assets/bundle.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>

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

    namespace
    {
        constexpr u32 BundleIndex{0};

        scene::primitive_kind convert_primitive_kind(int mode)
        {
            switch (mode)
            {
            case TINYGLTF_MODE_TRIANGLES:
                return scene::primitive_kind::triangle;
            default:
                OBLO_ASSERT(false);
                return scene::primitive_kind::enum_max;
            }
        }

        scene::data_format convert_data_format(int componentType)
        {
            scene::data_format format;

            switch (componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
                format = scene::data_format::i8;
                break;

            case TINYGLTF_COMPONENT_TYPE_SHORT:
                format = scene::data_format::i16;
                break;

            case TINYGLTF_COMPONENT_TYPE_INT:
                format = scene::data_format::i32;
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                format = scene::data_format::u8;
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                format = scene::data_format::u16;
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                format = scene::data_format::u32;
                break;

            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                format = scene::data_format::f32;
                break;

            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                format = scene::data_format::f64;
                break;

            default:
                format = scene::data_format::enum_max;
                break;
            }

            return format;
        }
    }

    gltf::gltf() = default;

    gltf::~gltf() = default;

    void gltf::init(const importer_config& config, import_preview& preview)
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
            return;
        }

        if (!success)
        {
            return;
        }

        preview.nodes.push_back(import_node{
            .type = get_type_id<scene::bundle>(),
            .name = config.sourceFile.filename().stem().string(),
        });

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
    }

    bool gltf::import(const import_context& ctx)
    {
        scene::bundle bundle;

        std::vector<scene::mesh_attribute> attributes;
        attributes.reserve(16);

        struct attribute_source
        {
            int accessor;
        };

        std::vector<attribute_source> sources;
        sources.reserve(32);

        std::vector<import_artifact> meshArtifacts;
        meshArtifacts.reserve(32);

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
                const auto primitiveKind = convert_primitive_kind(primitive.mode);

                if (primitiveKind == scene::primitive_kind::enum_max)
                {
                    continue;
                }

                attributes.clear();
                sources.clear();

                u32 vertexCount{0}, indexCount{0};

                if (const auto it = primitive.attributes.find("POSITION"); it != primitive.attributes.end())
                {
                    vertexCount = m_model.accessors[it->second].count;
                }
                else
                {
                    continue;
                }

                if (primitive.indices >= 0)
                {
                    const auto& accessor = m_model.accessors[primitive.indices];
                    indexCount = accessor.count;

                    const scene::data_format format = convert_data_format(accessor.componentType);

                    if (format == scene::data_format::enum_max)
                    {
                        continue;
                    }

                    attributes.push_back({
                        .kind = scene::attribute_kind::indices,
                        .format = format,
                    });

                    sources.emplace_back(primitive.indices);
                }

                for (auto& [attribute, accessor] : primitive.attributes)
                {
                    // TODO: Should probably read the accessor type instead of hardcoding the data format

                    if (attribute == "POSITION")
                    {
                        attributes.push_back({
                            .kind = scene::attribute_kind::position,
                            .format = scene::data_format::vec3,
                        });

                        sources.emplace_back(accessor);
                    }
                    else if (attribute == "NORMAL")
                    {
                        attributes.push_back({
                            .kind = scene::attribute_kind::normal,
                            .format = scene::data_format::vec3,
                        });

                        sources.emplace_back(accessor);
                    }
                }

                scene::mesh meshAsset;

                meshAsset.allocate(primitiveKind, vertexCount, indexCount, attributes);

                for (u32 i = 0; i < attributes.size(); ++i)
                {
                    const auto attributeKind = attributes[i].kind;
                    const std::span bytes = meshAsset.get_attribute(attributeKind);
                    const auto accessorIndex = sources[i].accessor;
                    const auto& accessor = m_model.accessors[accessorIndex];
                    const auto& bufferView = m_model.bufferViews[accessor.bufferView];
                    const auto& buffer = m_model.buffers[bufferView.buffer];

                    const auto* const data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

                    const auto expectedSize = tinygltf::GetComponentSizeInBytes(accessor.componentType) *
                                              tinygltf::GetNumComponentsInType(accessor.type) * accessor.count;

                    if (expectedSize != bytes.size())
                    {
                        return false;
                    }

                    std::memcpy(bytes.data(), data, bytes.size());
                }

                bundle.meshes.emplace_back(meshNodeConfig.id);
                modelAsset.meshes.emplace_back(meshNodeConfig.id);

                auto& meshArtifact = meshArtifacts.emplace_back();
                meshArtifact.id = meshNodeConfig.id;
                meshArtifact.data = any_asset{std::move(meshAsset)};
            }

            bundle.models.emplace_back(modelNodeConfig.id);

            ctx.importer->add_asset(
                {
                    .id = modelNodeConfig.id,
                    .data = any_asset{std::move(modelAsset)},
                },
                meshArtifacts);
        }

        ctx.importer->add_asset(
            {
                .id = ctx.importNodesConfig[BundleIndex].id,
                .data = any_asset{std::move(bundle)},
            },
            {});

        return true;
    }
}