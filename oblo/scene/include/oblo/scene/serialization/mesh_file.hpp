#pragma once

#include <filesystem>
#include <vector>

namespace tinygltf
{
    struct Model;
    struct Primitive;
}

namespace oblo::scene
{
    class mesh;
    struct mesh_attribute;

    using gltf_accessor = int;

    bool save_mesh(const mesh& mesh, const std::filesystem::path& destination);

    bool load_mesh(mesh& mesh,
                   const tinygltf::Model& model,
                   const tinygltf::Primitive& primitive,
                   std::vector<mesh_attribute>& attributes,
                   std::vector<gltf_accessor>& sources,
                   std::vector<bool>* usedBuffers);
}