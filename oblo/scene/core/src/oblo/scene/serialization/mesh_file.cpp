#include <oblo/scene/serialization/mesh_file.hpp>

#include <oblo/core/data_format.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/math/float.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/scene/resources/mesh.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

#include <fstream>
#include <span>

namespace oblo
{
    namespace
    {
        constexpr bool WriteBinaryGLB{true};
        constexpr bool PrettyFormatGLTF{!WriteBinaryGLB};
        constexpr bool AddScene{true}; // This is useful to look at the file in other software

        constexpr const char* ExtraAabb{"aabb"};
        constexpr const char* ExtraMeshlets{"meshlets"};

        constexpr std::string_view get_attribute_name(attribute_kind kind)
        {
            switch (kind)
            {
            case attribute_kind::position:
                return "POSITION";

            case attribute_kind::normal:
                return "NORMAL";

            case attribute_kind::indices:
                return "INDICES";

            case attribute_kind::microindices:
                return "MICROINDICES";

            case attribute_kind::uv0:
                return "TEXCOORD_0";

            case attribute_kind::tangent:
                return "TANGENT";

            case attribute_kind::bitangent:
                return "BITANGENT";

            default:
                unreachable();
            }
        }

        int get_primitive_mode(primitive_kind kind)
        {
            switch (kind)
            {
            case primitive_kind::triangle:
                return TINYGLTF_MODE_TRIANGLES;

            default:
                OBLO_ASSERT(false);
                return -1;
            }
        }

        int get_accessor_type(data_format format)
        {
            switch (format)
            {
            case data_format::i8:
            case data_format::i16:
            case data_format::i32:
            case data_format::i64:
            case data_format::u8:
            case data_format::u16:
            case data_format::u32:
            case data_format::u64:
            case data_format::f32:
            case data_format::f64:
                return TINYGLTF_TYPE_SCALAR;

            case data_format::vec2:
                return TINYGLTF_TYPE_VEC2;

            case data_format::vec3:
                return TINYGLTF_TYPE_VEC3;

            default:
                OBLO_ASSERT(false);
                return -1;
            }
        }

        primitive_kind convert_primitive_kind(int mode)
        {
            switch (mode)
            {
            case TINYGLTF_MODE_TRIANGLES:
                return primitive_kind::triangle;
            default:
                OBLO_ASSERT(false);
                return primitive_kind::enum_max;
            }
        }

        data_format convert_data_format(int componentType)
        {
            data_format format;

            switch (componentType)
            {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
                format = data_format::i8;
                break;

            case TINYGLTF_COMPONENT_TYPE_SHORT:
                format = data_format::i16;
                break;

            case TINYGLTF_COMPONENT_TYPE_INT:
                format = data_format::i32;
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                format = data_format::u8;
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                format = data_format::u16;
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                format = data_format::u32;
                break;

            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                format = data_format::f32;
                break;

            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                format = data_format::f64;
                break;

            default:
                format = data_format::enum_max;
                break;
            }

            return format;
        }

        template <typename F>
        void for_each_triangle(const mesh& m, F&& f)
        {
            if (m.get_primitive_kind() != primitive_kind::triangle)
            {
                return;
            }

            if (m.has_attribute(attribute_kind::indices))
            {
                switch (m.get_attribute_format(attribute_kind::indices))
                {
                case data_format::u8: {
                    const std::span indices = m.get_attribute<u8>(attribute_kind::indices);

                    for (u32 i = 0; i < m.get_index_count(); i += 3)
                    {
                        f(indices[i], indices[i + 1], indices[i + 2]);
                    }

                    break;
                }

                case data_format::u16: {
                    const std::span indices = m.get_attribute<u16>(attribute_kind::indices);

                    for (u32 i = 0; i < m.get_index_count(); i += 3)
                    {
                        f(indices[i], indices[i + 1], indices[i + 2]);
                    }

                    break;
                }

                case data_format::u32: {
                    const std::span indices = m.get_attribute<u32>(attribute_kind::indices);

                    for (u32 i = 0; i < m.get_index_count(); i += 3)
                    {
                        f(indices[i], indices[i + 1], indices[i + 2]);
                    }

                    break;
                }

                default:
                    break;
                }
            }
            else
            {
                for (u32 i = 0; i < m.get_vertex_count(); i += 3)
                {
                    f(i, i + 1, i + 2);
                }
            }
        }
    }

    bool save_mesh(const mesh& mesh, cstring_view destination)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;

        auto& gltfMesh = model.meshes.emplace_back();
        auto& primitive = gltfMesh.primitives.emplace_back();

        primitive.mode = get_primitive_mode(mesh.get_primitive_kind());

        tinygltf::Value::Object extras;

        if (const auto aabb = mesh.get_aabb(); is_valid(aabb))
        {
            using tinygltf::Value;

            extras[ExtraAabb] = Value{
                Value::Object{
                    {
                        "min",
                        Value{
                            Value::Array{
                                Value{aabb.min.x},
                                Value{aabb.min.y},
                                Value{aabb.min.z},
                            },
                        },
                    },
                    {
                        "max",
                        Value{
                            Value::Array{
                                Value{aabb.max.x},
                                Value{aabb.max.y},
                                Value{aabb.max.z},
                            },
                        },
                    },
                },
            };
        }

        const bool hasMeshlets = mesh.get_meshlet_count() != 0;
        const u32 numAttributes = mesh.get_attributes_count();
        model.accessors.reserve(numAttributes);
        model.buffers.reserve(numAttributes + u32{hasMeshlets});
        model.bufferViews.reserve(numAttributes + u32{hasMeshlets});

        for (u32 attributeIndex = 0; attributeIndex < numAttributes; ++attributeIndex)
        {
            const mesh_attribute attribute = mesh.get_attribute_at(attributeIndex);
            const std::span<const std::byte> attributeData = mesh.get_attribute(attribute.kind);

            const auto bufferViewId = int(model.bufferViews.size());
            const auto bufferId = int(model.buffers.size());
            const auto accessorId = int(model.accessors.size());

            auto& bufferView = model.bufferViews.emplace_back();
            bufferView.buffer = bufferId;
            bufferView.byteLength = attributeData.size();

            auto& buffer = model.buffers.emplace_back();

            const std::span<const unsigned char> attributeDataUChar{
                reinterpret_cast<const unsigned char*>(attributeData.data()),
                attributeData.size()};

            const auto attributeName = get_attribute_name(attribute.kind);

            buffer.data.assign(attributeDataUChar.begin(), attributeDataUChar.end());
            buffer.name = attributeName;

            auto& accessor = model.accessors.emplace_back();
            accessor.count = mesh.get_elements_count(attribute.kind);
            accessor.bufferView = bufferViewId;
            accessor.name = attributeName;
            accessor.type = get_accessor_type(attribute.format);

            if (attribute.kind == attribute_kind::indices)
            {
                primitive.indices = accessorId;
            }
            else
            {
                primitive.attributes[accessor.name] = accessorId;
            }

            int componentType;

            switch (attribute.format)
            {
            case data_format::i8:
                componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
                break;

            case data_format::i16:
                componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
                break;

            case data_format::i32:
                componentType = TINYGLTF_COMPONENT_TYPE_INT;
                break;

            case data_format::u8:
                componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                break;

            case data_format::u16:
                componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                break;

            case data_format::u32:
                componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                break;

            case data_format::f32:
            case data_format::vec2:
            case data_format::vec3:
                componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                break;

            case data_format::f64:
                componentType = TINYGLTF_COMPONENT_TYPE_DOUBLE;
                break;

            default:
                OBLO_ASSERT(false);
                componentType = -1;
                break;
            }

            accessor.componentType = componentType;
        }

        if (hasMeshlets)
        {
            const auto meshlets = mesh.get_meshlets();
            const auto meshletBytes = as_bytes(meshlets);

            const auto bufferViewId = int(model.bufferViews.size());
            const auto bufferId = int(model.buffers.size());

            auto& bufferView = model.bufferViews.emplace_back();
            bufferView.buffer = bufferId;
            bufferView.byteLength = meshletBytes.size();

            auto& buffer = model.buffers.emplace_back();

            const std::span<const unsigned char> meshletDataUChar{
                reinterpret_cast<const unsigned char*>(meshletBytes.data()),
                meshletBytes.size()};

            buffer.data.assign(meshletDataUChar.begin(), meshletDataUChar.end());
            buffer.name = ExtraMeshlets;

            using tinygltf::Value;

            extras[ExtraMeshlets] = Value{
                Value::Object{
                    {
                        "count",
                        Value{i32(meshlets.size())},
                    },
                    {
                        "bufferViewId",
                        Value{bufferViewId},
                    },
                },
            };
        }

        if constexpr (AddScene)
        {
            auto& root = model.nodes.emplace_back();
            root.name = "Mesh";
            root.mesh = 0;

            auto& scene = model.scenes.emplace_back();
            model.defaultScene = 0;

            scene.nodes.emplace_back(0);
        }

        gltfMesh.extras = tinygltf::Value{std::move(extras)};

        std::ofstream ofs{destination.as<std::string>(), std::ios::binary};

        if (!ofs)
        {
            return false;
        }

        return loader.WriteGltfSceneToStream(&model, ofs, PrettyFormatGLTF, WriteBinaryGLB);
    }

    bool load_mesh(mesh& mesh,
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        dynamic_array<mesh_attribute>& attributes,
        dynamic_array<gltf_accessor>& sources,
        dynamic_array<bool>* usedBuffers,
        flags<mesh_post_process> processingFlags)
    {
        const auto primitiveKind = convert_primitive_kind(primitive.mode);

        if (primitiveKind == primitive_kind::enum_max)
        {
            return false;
        }

        attributes.clear();
        sources.clear();

        u32 vertexCount{0}, indexCount{0};

        if (const auto it = primitive.attributes.find(std::string{get_attribute_name(attribute_kind::position)});
            it != primitive.attributes.end())
        {
            vertexCount = narrow_cast<u32>(model.accessors[u32(it->second)].count);
        }
        else
        {
            return false;
        }

        if (primitive.indices >= 0)
        {
            const auto& accessor = model.accessors[primitive.indices];
            indexCount = narrow_cast<u32>(accessor.count);

            const data_format format = convert_data_format(accessor.componentType);

            if (format == data_format::enum_max)
            {
                return false;
            }

            attributes.push_back({
                .kind = attribute_kind::indices,
                .format = format,
            });

            sources.emplace_back(primitive.indices);
        }

        for (auto& [attribute, accessor] : primitive.attributes)
        {
            // TODO: Should probably read the accessor type instead of hardcoding the data format

            if (attribute == get_attribute_name(attribute_kind::position))
            {
                attributes.push_back({
                    .kind = attribute_kind::position,
                    .format = data_format::vec3,
                });

                sources.emplace_back(accessor);
            }
            else if (attribute == get_attribute_name(attribute_kind::normal))
            {
                attributes.push_back({
                    .kind = attribute_kind::normal,
                    .format = data_format::vec3,
                });

                sources.emplace_back(accessor);
            }
            else if (attribute == get_attribute_name(attribute_kind::tangent))
            {
                attributes.push_back({
                    .kind = attribute_kind::tangent,
                    .format = data_format::vec3,
                });

                sources.emplace_back(accessor);
            }
            else if (attribute == get_attribute_name(attribute_kind::bitangent))
            {
                attributes.push_back({
                    .kind = attribute_kind::bitangent,
                    .format = data_format::vec3,
                });

                sources.emplace_back(accessor);
            }
            else if (attribute == get_attribute_name(attribute_kind::uv0))
            {
                attributes.push_back({
                    .kind = attribute_kind::uv0,
                    .format = data_format::vec2,
                });

                sources.emplace_back(accessor);
            }
            else if (attribute == get_attribute_name(attribute_kind::microindices))
            {
                attributes.push_back({
                    .kind = attribute_kind::microindices,
                    .format = data_format::u8,
                });

                sources.emplace_back(accessor);
            }
        }

        const auto numAttributesFromFile = attributes.size();

        flags<attribute_kind> attributesFromFile;

        for (auto& attribute : attributes)
        {
            attributesFromFile |= attribute.kind;
        }

        bool generateTangentSpace{};

        if (processingFlags.contains(mesh_post_process::generate_tanget_space) &&
            attributesFromFile.contains(attribute_kind::uv0) && attributesFromFile.contains(attribute_kind::position))
        {
            if (!attributesFromFile.contains(attribute_kind::tangent))
            {
                attributes.push_back({.kind = attribute_kind::tangent, .format = data_format::vec3});
                generateTangentSpace = true;
            }

            if (!attributesFromFile.contains(attribute_kind::bitangent))
            {
                attributes.push_back({.kind = attribute_kind::bitangent, .format = data_format::vec3});
                generateTangentSpace = true;
            }
        }

        mesh.allocate(primitiveKind, vertexCount, indexCount, 0, attributes);

        int vec4TangentAcessor = -1;

        for (u32 i = 0; i < numAttributesFromFile; ++i)
        {
            const auto attributeKind = attributes[i].kind;
            const std::span bytes = mesh.get_attribute(attributeKind);
            const auto accessorIndex = sources[i];
            const auto& accessor = model.accessors[accessorIndex];
            const auto& bufferView = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            if (usedBuffers)
            {
                usedBuffers->at(bufferView.buffer) = true;
            }

            const auto* const data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

            const auto expectedSize = tinygltf::GetComponentSizeInBytes(accessor.componentType) *
                tinygltf::GetNumComponentsInType(accessor.type) * accessor.count;

            if (expectedSize == bytes.size())
            {
                std::memcpy(bytes.data(), data, bytes.size());
            }
            else if (attributeKind == attribute_kind::tangent &&
                accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC4)
            {
                vec4TangentAcessor = accessorIndex;
                continue;
            }
            else
            {
                return false;
            }
        }

        if (vec4TangentAcessor >= 0 && mesh.has_attribute(attribute_kind::normal))
        {
            const auto& accessor = model.accessors[vec4TangentAcessor];

            const auto& bufferView = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];

            const auto tangents = mesh.get_attribute<vec3>(attribute_kind::tangent);
            const auto bitangents = mesh.get_attribute<vec3>(attribute_kind::bitangent);
            const auto normals = mesh.get_attribute<vec3>(attribute_kind::normal);

            if (tangents.size() != accessor.count || normals.size() != accessor.count ||
                (bitangents.size() != accessor.count && !bitangents.empty()))
            {
                return false;
            }

            const auto* const src =
                start_lifetime_as_array<vec4>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset,
                    accessor.count);

            for (u32 i = 0; i < normals.size(); ++i)
            {
                const auto srcTangent = src[i];
                tangents[i] = {srcTangent.x, srcTangent.y, srcTangent.z};

                if (!bitangents.empty())
                {
                    bitangents[i] = normalize(cross(normals[i], tangents[i]) * srcTangent.w);
                }
            }
        }
        else if (generateTangentSpace)
        {
            const auto uv0 = mesh.get_attribute<vec2>(attribute_kind::uv0);
            const auto positions = mesh.get_attribute<vec3>(attribute_kind::position);
            const auto tangents = mesh.get_attribute<vec3>(attribute_kind::tangent);
            const auto bitangents = mesh.get_attribute<vec3>(attribute_kind::bitangent);
            const auto normals = mesh.get_attribute<vec3>(attribute_kind::normal);

            for_each_triangle(mesh,
                [positions, uv0, tangents, bitangents, normals](u32 v0, u32 v1, u32 v2)
                {
                    // Edges of the triangle : position delta
                    const vec3 deltaPos1 = positions[v1] - positions[v0];
                    const vec3 deltaPos2 = positions[v2] - positions[v0];

                    // UV delta
                    const vec2 deltaUV1 = uv0[v1] - uv0[v0];
                    const vec2 deltaUV2 = uv0[v2] - uv0[v0];

                    const f32 r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
                    const vec3 t = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;

                    for (const auto v : {v0, v1, v2})
                    {
                        const vec3 normal = normals[v];

                        // Gram-Schmidt orthogonalization (i.e. project onto the plane formed by the normal)
                        const vec3 tangent = normalize(t - normal * dot(normal, t));
                        const vec3 bitangent = normalize(cross(normal, tangent));
                        // const vec3 bitangent = normalize(b - normal * dot(normal, b));

                        // const vec3 bitangent = normalize(cross(normal, tangent));
                        // const auto direction = dot(normal, cross(tangent, bitangent));

                        tangents[v] = tangent;
                        bitangents[v] = bitangent;
                        // bitangents[v] = direction < 0 ? -bitangent : bitangent;

                        // const auto n2 = normalize(cross(tangents[v], bitangents[v]));
                        // const auto ndt = dot(n2, normals[v]);
                        // OBLO_ASSERT(float_equal(ndt, 1.f, 0.01f));
                    }
                });
        }

        return true;
    }

    bool load_mesh(mesh& mesh, cstring_view source)
    {
        std::ifstream ifs{source.as<std::string>(), std::ios::ate | std::ios::binary};

        if (!ifs)
        {
            return false;
        }

        const auto fileSize = narrow_cast<u32>(ifs.tellg());

        constexpr auto MagicCharsCount{4};

        if (MagicCharsCount > fileSize)
        {
            return false;
        }

        char magic[MagicCharsCount];

        ifs.seekg(0);
        ifs.read(magic, MagicCharsCount);

        if (!ifs)
        {
            return false;
        }

        ifs.seekg(0);

        std::vector<char> content;
        content.resize(fileSize);

        ifs.read(content.data(), fileSize);

        tinygltf::TinyGLTF loader;
        loader.SetStoreOriginalJSONForExtrasAndExtensions(true);

        tinygltf::Model model;

        const auto parentPath = filesystem::parent_path(source).as<std::string>();

        std::string err, warn;

        bool success;

        if (std::string_view{magic, MagicCharsCount} == "glTF")
        {
            success = loader.LoadBinaryFromMemory(&model,
                &err,
                &warn,
                reinterpret_cast<const unsigned char*>(content.data()),
                fileSize,
                parentPath);
        }
        else
        {
            success = loader.LoadASCIIFromString(&model, &err, &warn, content.data(), fileSize, parentPath);
        }

        if (!success)
        {
            return false;
        }

        if (model.meshes.size() != 1 || model.meshes[0].primitives.size() != 1)
        {
            return false;
        }

        const auto& primitive = model.meshes[0].primitives[0];

        // We count indices as attributes here
        const auto maxAttributes = primitive.attributes.size() + 1;

        dynamic_array<mesh_attribute> attributes;
        attributes.reserve(maxAttributes);

        dynamic_array<gltf_accessor> sources;
        sources.reserve(maxAttributes);

        if (load_mesh(mesh, model, primitive, attributes, sources, nullptr, {}))
        {
            aabb aabb = aabb::make_invalid();

            if (const auto& extra = model.meshes[0].extras_json_string; !extra.empty())
            {
                using tinygltf::Value;

                const auto json = tinygltf::detail::json::parse(extra, nullptr, false);

                Value extraValue;

                if (!json.is_discarded() && tinygltf::ParseJsonAsValue(&extraValue, json))
                {
                    auto& aabbValue = extraValue.Get(ExtraAabb);

                    if (aabbValue.IsObject())
                    {
                        auto& minValue = aabbValue.Get("min");
                        auto& maxValue = aabbValue.Get("max");

                        for (u32 i = 0; i < 3; ++i)
                        {
                            aabb.min[i] = f32(minValue.Get(i).GetNumberAsDouble());
                            aabb.max[i] = f32(maxValue.Get(i).GetNumberAsDouble());
                        }
                    }

                    auto& meshletsValue = extraValue.Get(ExtraMeshlets);

                    if (meshletsValue.IsObject())
                    {
                        auto& countValue = meshletsValue.Get("count");
                        auto& bufferViewIdValue = meshletsValue.Get("bufferViewId");

                        const u32 meshletCount = u32(countValue.GetNumberAsInt());
                        const u32 bufferViewId = u32(bufferViewIdValue.GetNumberAsInt());

                        mesh.reset_meshlets(meshletCount);

                        if (bufferViewId < model.bufferViews.size())
                        {
                            const auto dstMeshlets = mesh.get_meshlets();

                            auto& bufferView = model.bufferViews[bufferViewId];
                            OBLO_ASSERT(bufferView.byteLength == dstMeshlets.size_bytes());

                            if (bufferView.byteLength == dstMeshlets.size_bytes())
                            {
                                auto& buffer = model.buffers[bufferView.buffer];

                                std::memcpy(dstMeshlets.data(),
                                    buffer.data.data() + bufferView.byteOffset,
                                    bufferView.byteLength);
                            }
                        }
                    }
                }
            }

            mesh.set_aabb(aabb);

            return true;
        }

        return false;
    }
}