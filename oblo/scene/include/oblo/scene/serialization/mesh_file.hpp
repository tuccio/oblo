#pragma once

#include <filesystem>
#include <vector>

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
        std::vector<mesh_attribute>& attributes,
        std::vector<gltf_accessor>& sources,
        std::vector<bool>* usedBuffers);

    SCENE_API bool load_mesh(mesh& mesh, const std::filesystem::path& source);
}