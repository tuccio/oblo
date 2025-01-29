#include <gtest/gtest.h>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/registration.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/thread/job_manager.hpp>

namespace oblo::importers
{
    namespace
    {
        void clear_directory(cstring_view path)
        {
            filesystem::remove_all(path).assert_value();
        }

        template <typename... T>
        string child_path(string_view parent, T&&... children)
        {
            string_builder sb;
            sb.append(parent);

            (sb.append_path(children), ...);

            return sb.as<string>();
        }
    }

    TEST(gltf_importer, box)
    {
        job_manager jm;
        jm.init();

        const auto cleanUp = finally([&] { jm.shutdown(); });

        module_manager mm;
        mm.load<scene_module>();

        resource_registry resources;

        asset_registry registry;

        const string testDir{"./test/gltf_importer_suzanne/"};
        const string assetsDir{child_path(testDir, "assets")};
        const string artifactsDir{child_path(testDir, "artifacts")};
        const string sourceFilesDir{child_path(testDir, "sourcefiles")};

        clear_directory(testDir);

        ASSERT_TRUE(registry.initialize(assetsDir, artifactsDir, sourceFilesDir));
        ASSERT_TRUE(registry.initialize_directory_watcher());

        deque<resource_type_descriptor> resourceTypes;
        fetch_scene_resource_types(resourceTypes);

        for (const auto& type : resourceTypes)
        {
            resources.register_type(type);
        }

        register_gltf_importer(registry);

        resources.register_provider(registry.initialize_resource_provider());

        const string gltfSampleModels{OBLO_GLTF_SAMPLE_MODELS};

        const string files[] = {
            child_path(gltfSampleModels, "Models", "Box", "glTF-Embedded", "Box.gltf"),
            child_path(gltfSampleModels, "Models", "Box", "glTF", "Box.gltf"),
            child_path(gltfSampleModels, "Models", "Box", "glTF-Binary", "Box.glb"),
        };

        for (const auto& file : files)
        {
            const auto dirName = filesystem::filename(filesystem::parent_path(file));

            data_document importSettings;
            importSettings.init();
            importSettings.child_value(importSettings.get_root(),
                "generateMeshlets"_hsv,
                property_value_wrapper{false});

            const auto importResult = registry.import(file, dirName, "Box", std::move(importSettings));

            ASSERT_TRUE(importResult);

            while (registry.get_running_import_count() > 0)
            {
                registry.update();
            }

            resources.update();

            uuid meshId;

            asset_meta modelMeta;

            string_builder assetPath;
            assetPath.append(dirName);
            assetPath.append_path("Box");

            ASSERT_TRUE(registry.find_asset_by_path(assetPath, meshId, modelMeta));

            ASSERT_NE(modelMeta.mainArtifactHint, uuid{});
            ASSERT_EQ(modelMeta.typeHint, resource_type<model>);

            const auto modelResource = resources.get_resource(modelMeta.mainArtifactHint).as<model>();
            ASSERT_TRUE(modelResource);

            modelResource.load_sync();

            ASSERT_EQ(modelResource->meshes.size(), 1);

            const resource_ref meshRef = modelResource->meshes[0];
            ASSERT_TRUE(meshRef);

            const auto meshResource = resources.get_resource(meshRef.id).as<mesh>();
            ASSERT_TRUE(meshResource);

            meshResource.load_sync();

            constexpr auto expectedVertexCount{24};
            constexpr auto expectedIndexCount{36};

            ASSERT_EQ(meshResource->get_primitive_kind(), primitive_kind::triangle);
            ASSERT_EQ(meshResource->get_vertex_count(), expectedVertexCount);
            ASSERT_EQ(meshResource->get_index_count(), expectedIndexCount);

            ASSERT_EQ(meshResource->get_attribute_format(attribute_kind::position), data_format::vec3);
            ASSERT_EQ(meshResource->get_attribute_format(attribute_kind::normal), data_format::vec3);
            ASSERT_EQ(meshResource->get_attribute_format(attribute_kind::indices), data_format::u16);

            const std::span positions = meshResource->get_attribute<vec3>(attribute_kind::position);
            const std::span normals = meshResource->get_attribute<vec3>(attribute_kind::normal);
            const std::span indices = meshResource->get_attribute<u16>(attribute_kind::indices);

            ASSERT_EQ(positions.size(), expectedVertexCount);
            ASSERT_EQ(normals.size(), expectedVertexCount);
            ASSERT_EQ(indices.size(), expectedIndexCount);

            const auto aabb = meshResource->get_aabb();
            ASSERT_TRUE(is_valid(aabb));

            ASSERT_NEAR(aabb.min.x, -.5f, .0001f);
            ASSERT_NEAR(aabb.min.y, -.5f, .0001f);
            ASSERT_NEAR(aabb.min.z, -.5f, .0001f);

            ASSERT_NEAR(aabb.max.x, .5f, .0001f);
            ASSERT_NEAR(aabb.max.y, .5f, .0001f);
            ASSERT_NEAR(aabb.max.z, .5f, .0001f);
        }
    }
}