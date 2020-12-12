#include <sandbox/import/scene_importer.hpp>

#include <oblo/rendering/raytracer.hpp>
#include <sandbox/sandbox_state.hpp>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <memory>

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

        for (u32 i = 0; i < scene->mNumMeshes; ++i)
        {
        }

        return true;
    }

}