#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>

#include <span>

namespace tinygltf
{
    class Model;
    struct Primitive;
}

namespace oblo
{
    class cstring_view;
    class mesh;
    struct mesh_attribute;

    using gltf_accessor = int;

    enum class mesh_post_process : u8
    {
        generate_tanget_space,
        enum_max,
    };

    SCENE_API bool save_mesh(const mesh& mesh, cstring_view destination);

    SCENE_API bool load_mesh(mesh& mesh,
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        dynamic_array<mesh_attribute>& attributes,
        dynamic_array<gltf_accessor>& sources,
        const std::span<bool>* usedBuffers,
        flags<mesh_post_process> processingFlags);

    SCENE_API bool load_mesh(mesh& mesh, cstring_view source);
}