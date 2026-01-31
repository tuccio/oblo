#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/forward.hpp>

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

    OBLO_SCENE_API expected<> save_mesh(const mesh& mesh, cstring_view destination);

    OBLO_SCENE_API expected<> load_mesh(mesh& mesh,
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        dynamic_array<mesh_attribute>& attributes,
        dynamic_array<gltf_accessor>& sources,
        const std::span<bool>* usedBuffers,
        flags<mesh_post_process> processingFlags);

    OBLO_SCENE_API expected<> load_mesh(mesh& mesh, cstring_view source);
}