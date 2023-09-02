#include <oblo/scene/serialization/mesh_file.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/scene/assets/mesh.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tiny_gltf.h>

namespace oblo::scene
{
    namespace
    {
        constexpr bool WriteBinaryGLB{true};
        constexpr bool PrettyFormatGLTF{!WriteBinaryGLB};
        constexpr bool AddScene{true}; // This is useful to look at the file in other software

        std::string_view get_attribute_name(attribute_kind kind)
        {
            switch (kind)
            {
            case attribute_kind::position:
                return "POSITION";

            case attribute_kind::normal:
                return "NORMAL";

            case attribute_kind::indices:
                return "INDICES";

            default:
                OBLO_ASSERT(false);
                return {};
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
    }

    bool save_mesh(const mesh& mesh, const std::filesystem::path& destination)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;

        auto& gltfMesh = model.meshes.emplace_back();
        auto& primitive = gltfMesh.primitives.emplace_back();

        primitive.mode = get_primitive_mode(mesh.get_primitive_kind());

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
            case scene::data_format::i8:
                componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
                break;

            case scene::data_format::i16:
                componentType = TINYGLTF_COMPONENT_TYPE_SHORT;
                break;

            case scene::data_format::i32:
                componentType = TINYGLTF_COMPONENT_TYPE_INT;
                break;

            case scene::data_format::u8:
                componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                break;

            case scene::data_format::u16:
                componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                break;

            case scene::data_format::u32:
                componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                break;

            case scene::data_format::f32:
            case scene::data_format::vec2:
            case scene::data_format::vec3:
                componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                break;

            case scene::data_format::f64:
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
}