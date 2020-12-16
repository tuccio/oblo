#include <sandbox/import/scene_importer.hpp>

#include <oblo/rendering/raytracer.hpp>
#include <sandbox/sandbox_state.hpp>

#include <memory>

// TODO: Check warnings coming from assimp includes
#pragma clang diagnostic ignored "-Wpragma-pack"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace oblo
{
    bool scene_importer::import(sandbox_state& state, const std::filesystem::path& filename)
    {
        auto scene = std::unique_ptr<const aiScene, decltype(&aiReleaseImport)>{
            aiImportFile(filename.string().c_str(),
                         aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                             aiProcess_SortByPType),
            aiReleaseImport};

        if (!scene)
        {
            return false;
        }

        state.raytracer->clear();

        for (u32 meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            aiMesh* mesh = scene->mMeshes[meshIndex];
            OBLO_ASSERT(mesh->HasFaces());

            if (!mesh->HasFaces())
            {
                return false;
            }

            triangle_container triangles;
            triangles.reserve(mesh->mNumFaces);

            for (const aiFace& face : std::span{mesh->mFaces, mesh->mNumFaces})
            {
                OBLO_ASSERT(face.mNumIndices == 3);
                const auto indices = face.mIndices;

                const auto& v0 = mesh->mVertices[indices[0]];
                const auto& v1 = mesh->mVertices[indices[1]];
                const auto& v2 = mesh->mVertices[indices[2]];

                triangle t;
                t.v[0] = {narrow_cast<float>(v0.x), narrow_cast<float>(v0.y), narrow_cast<float>(v0.z)};
                t.v[1] = {narrow_cast<float>(v1.x), narrow_cast<float>(v1.y), narrow_cast<float>(v1.z)};
                t.v[2] = {narrow_cast<float>(v2.x), narrow_cast<float>(v2.y), narrow_cast<float>(v2.z)};

                triangles.add({&t, 1});
            }

            state.raytracer->add_mesh(std::move(triangles));
        }

        struct stack_info
        {
            aiNode* node;
        };

        std::vector<stack_info> nodes;
        nodes.reserve(64);

        nodes.push_back({scene->mRootNode});

        while (!nodes.empty())
        {
            [[maybe_unused]] const stack_info& current = nodes.back();

            // TODO: Evaluate transforms
            // TOOD: Support for mesh instances

            nodes.pop_back();
        }

        state.raytracer->rebuild_tlas();

        return true;
    }

}