#include <gtest/gtest.h>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/asset/meta.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/ptr.hpp>
#include <oblo/resource/registry.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/registration.hpp>

namespace oblo::asset::importers
{
    namespace
    {
        void clear_directory(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            ASSERT_FALSE(ec);
        }
    }

    TEST(gltf_importer, box)
    {
        resource::registry resources;
        scene::register_resource_types(resources);

        asset_registry registry;

        const std::filesystem::path testDir{"./test/gltf_importer_suzanne/"};
        const std::filesystem::path assetsDir{testDir / "assets"};
        const std::filesystem::path artifactsDir{testDir / "artifacts"};
        const std::filesystem::path sourceFilesDir{testDir / "sourcefiles"};

        clear_directory(testDir);

        ASSERT_TRUE(registry.initialize(assetsDir, artifactsDir, sourceFilesDir));
        scene::register_asset_types(registry);

        register_gltf_importer(registry);

        resources.register_provider(&asset_registry::find_artifact_resource, &registry);

        const std::filesystem::path gltfSampleModels{OBLO_GLTF_SAMPLE_MODELS};

        const std::filesystem::path files[] = {
            gltfSampleModels / "2.0" / "Box" / "glTF-Embedded" / "Box.gltf",
            gltfSampleModels / "2.0" / "Box" / "glTF" / "Box.gltf",
            gltfSampleModels / "2.0" / "Box" / "glTF-Binary" / "Box.glb",
        };

        for (const auto& file : files)
        {
            auto importer = registry.create_importer(file);

            const auto dirName = file.parent_path().filename();

            ASSERT_TRUE(importer.is_valid());

            ASSERT_TRUE(importer.init());
            ASSERT_TRUE(importer.execute(dirName));

            uuid meshId;

            asset_meta modelMeta;

            ASSERT_TRUE(registry.find_asset_by_path(dirName / "Mesh", meshId, modelMeta));

            ASSERT_EQ(modelMeta.type, get_type_id<scene::model>());

            const auto modelResource = resources.get_resource(modelMeta.id).as<scene::model>();
            ASSERT_TRUE(modelResource);

            ASSERT_EQ(modelResource->meshes.size(), 1);

            const asset::ref meshRef = modelResource->meshes[0];
            ASSERT_TRUE(meshRef);

            const auto meshResource = resources.get_resource(meshRef.id).as<scene::mesh>();
            ASSERT_TRUE(meshResource);

            constexpr auto expectedVertexCount{24};
            constexpr auto expectedIndexCount{36};

            ASSERT_EQ(meshResource->get_primitive_kind(), scene::primitive_kind::triangle);
            ASSERT_EQ(meshResource->get_vertex_count(), expectedVertexCount);
            ASSERT_EQ(meshResource->get_index_count(), expectedIndexCount);

            ASSERT_EQ(meshResource->get_attribute_format(scene::attribute_kind::position), scene::data_format::vec3);
            ASSERT_EQ(meshResource->get_attribute_format(scene::attribute_kind::normal), scene::data_format::vec3);
            ASSERT_EQ(meshResource->get_attribute_format(scene::attribute_kind::indices), scene::data_format::u16);

            const std::span positions = meshResource->get_attribute<vec3>(scene::attribute_kind::position);
            const std::span normals = meshResource->get_attribute<vec3>(scene::attribute_kind::normal);
            const std::span indices = meshResource->get_attribute<u16>(scene::attribute_kind::indices);

            ASSERT_EQ(positions.size(), expectedVertexCount);
            ASSERT_EQ(normals.size(), expectedVertexCount);
            ASSERT_EQ(indices.size(), expectedIndexCount);
        }
    }
}