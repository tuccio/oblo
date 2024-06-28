#include <oblo/scene/serialization/mesh_file.hpp>

#include <oblo/core/data_format.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/scene/assets/mesh.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

#include <fstream>

namespace oblo
{
    namespace
    {
        constexpr bool WriteBinaryGLB{true};
        constexpr bool PrettyFormatGLTF{!WriteBinaryGLB};
        constexpr bool AddScene{true}; // This is useful to look at the file in other software

        constexpr const char* ExtraAbb{"aabb"};

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

            case attribute_kind::uv0:
                return "TEXCOORD_0";

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
    }

    bool save_mesh(const mesh& mesh, const std::filesystem::path& destination)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;

        auto& gltfMesh = model.meshes.emplace_back();
        auto& primitive = gltfMesh.primitives.emplace_back();

        primitive.mode = get_primitive_mode(mesh.get_primitive_kind());

        if (const auto aabb = mesh.get_aabb(); is_valid(aabb))
        {
            using tinygltf::Value;

            gltfMesh.extras = Value{
                Value::Object{
                    {
                        ExtraAbb,
                        Value{
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
                        },
                    },
                },
            };
        }

        const u32 numAttributes = mesh.get_attributes_count();
        model.accessors.reserve(numAttributes);
        model.buffers.reserve(numAttributes);
        model.bufferViews.reserve(numAttributes);

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

        if constexpr (AddScene)
        {
            auto& root = model.nodes.emplace_back();
            root.name = "Mesh";
            root.mesh = 0;

            auto& scene = model.scenes.emplace_back();
            model.defaultScene = 0;

            scene.nodes.emplace_back(0);
        }

        std::ofstream ofs{destination, std::ios::binary};

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
        dynamic_array<bool>* usedBuffers)
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
            else if (attribute == get_attribute_name(attribute_kind::uv0))
            {
                attributes.push_back({
                    .kind = attribute_kind::uv0,
                    .format = data_format::vec2,
                });

                sources.emplace_back(accessor);
            }
        }

        mesh.allocate(primitiveKind, vertexCount, indexCount, 0, attributes);

        for (u32 i = 0; i < attributes.size(); ++i)
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

            if (expectedSize != bytes.size())
            {
                return false;
            }

            std::memcpy(bytes.data(), data, bytes.size());
        }

        return true;
    }

    bool load_mesh(mesh& mesh, const std::filesystem::path& source)
    {
        std::ifstream ifs{source, std::ios::ate | std::ios::binary};

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

        const auto parentPath = source.parent_path().string();

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

        if (load_mesh(mesh, model, primitive, attributes, sources, nullptr))
        {
            aabb aabb = aabb::make_invalid();

            if (const auto& extra = model.meshes[0].extras_json_string; !extra.empty())
            {
                using tinygltf::Value;

                const auto json = tinygltf::detail::json::parse(extra, nullptr, false);

                Value extraValue;

                if (!json.is_discarded() && tinygltf::ParseJsonAsValue(&extraValue, json))
                {
                    auto& aabbValue = extraValue.Get(ExtraAbb);

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
                }
            }

            mesh.set_aabb(aabb);

            return true;
        }

        return false;
    }
}