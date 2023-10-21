#include <sandbox/import/scene_importer.hpp>

#include <oblo/raytracer/camera.hpp>
#include <oblo/raytracer/material.hpp>
#include <oblo/raytracer/raytracer.hpp>
#include <sandbox/state/sandbox_state.hpp>

#include <memory>

// TODO: Check warnings coming from assimp includes
#pragma clang diagnostic ignored "-Wpragma-pack"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace oblo
{
    namespace
    {
        vec3 convert_vec3(const aiColor3D& v)
        {
            return {narrow_cast<float>(v.r), narrow_cast<float>(v.g), narrow_cast<float>(v.b)};
        }

        vec3 convert_vec3(const aiVector3D& v)
        {
            return {narrow_cast<float>(v.x), narrow_cast<float>(v.y), narrow_cast<float>(v.z)};
        }

        vec3 get_color3_or_default(const aiMaterial* material, const char* name, int type, int idx, vec3 def)
        {
            aiColor3D color;

            if (material->Get(name, type, idx, color) != aiReturn_SUCCESS)
            {
                return def;
            }

            return convert_vec3(color);
        }
    }

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

        if (scene->mNumCameras > 0)
        {
            const aiCamera& camera = *scene->mCameras[0];

            camera_set_look_at(state.camera,
                convert_vec3(camera.mPosition),
                convert_vec3(camera.mLookAt),
                convert_vec3(camera.mUp));

            camera_set_horizontal_fov(state.camera, radians{camera.mHorizontalFOV});

            state.camera.near = 0.1f;
            state.camera.far = 100.f;
        }

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
                t.v[0] = convert_vec3(v0);
                t.v[1] = convert_vec3(v1);
                t.v[2] = convert_vec3(v2);

                triangles.add({&t, 1});
            }

            [[maybe_unused]] u32 newMesh = state.raytracer->add_mesh(std::move(triangles));
            OBLO_ASSERT(meshIndex == newMesh);
        }

        for (u32 materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
        {
            const aiMaterial* sceneMaterial = scene->mMaterials[materialIndex];

            material material{};

            material.albedo = get_color3_or_default(sceneMaterial, AI_MATKEY_COLOR_DIFFUSE, {});
            material.emissive = get_color3_or_default(sceneMaterial, AI_MATKEY_COLOR_EMISSIVE, {});

            [[maybe_unused]] u32 newMaterial = state.raytracer->add_material(material);
            OBLO_ASSERT(newMaterial == materialIndex);
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
            const stack_info& current = nodes.back();

            // TODO: Evaluate transforms
            for (u32 meshIndex : std::span{current.node->mMeshes, current.node->mNumMeshes})
            {
                const aiMesh* mesh = scene->mMeshes[meshIndex];
                state.raytracer->add_instance({meshIndex, mesh->mMaterialIndex});
            }

            const auto children = std::span{current.node->mChildren, current.node->mNumChildren};

            nodes.pop_back();

            for (const auto child : children)
            {
                nodes.push_back({child});
            }
        }

        state.raytracer->rebuild_tlas();

        return true;
    }

}