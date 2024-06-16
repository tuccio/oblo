#pragma once

#include <oblo/core/dynamic_array.hpp>

#include <filesystem>

namespace tinygltf
{
    class Model;
    struct Primitive;
}

namespace oblo
{
    class mesh;
    struct mesh_attribute;

    using gltf_accessor = int;

    SCENE_API bool save_mesh(const mesh& mesh, const std::filesystem::path& destination);

    SCENE_API bool load_mesh(mesh& mesh,
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        dynamic_array<mesh_attribute>& attributes,
        dynamic_array<gltf_accessor>& sources,
        dynamic_array<bool>* usedBuffers);

    SCENE_API bool load_mesh(mesh& mesh, const std::filesystem::path& source);
}