#include <gtest/gtest.h>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_path.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/descriptors/asset_repository_descriptor.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/core/data_format.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/registration.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
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

        class test_module : public module_interface
        {
        public:
            bool startup(const module_initializer& initializer)
            {
                m_jobManager.init();

                auto& mm = module_manager::get();
                mm.load<graphics_module>();
                mm.load<scene_module>();
                auto* reflection = mm.load<reflection::reflection_module>();
                m_propertyRegistry.init(reflection->get_registry());

                initializer.services->add<const reflection::reflection_registry>().externally_owned(
                    &reflection->get_registry());

                initializer.services->add<const property_registry>().externally_owned(&m_propertyRegistry);
                initializer.services->add<const ecs::type_registry>().externally_owned(&m_typeRegistry);

                return true;
            }

            bool finalize()
            {
                auto& mm = module_manager::get();
                auto* reflection = mm.find<reflection::reflection_module>();

                ecs_utility::register_reflected_component_and_tag_types(reflection->get_registry(),
                    &m_typeRegistry,
                    &m_propertyRegistry);

                return true;
            }

            void shutdown()
            {
                m_jobManager.shutdown();
            }

        private:
            job_manager m_jobManager;
            property_registry m_propertyRegistry;
            ecs::type_registry m_typeRegistry;
        };

        template <typename T>
        resource_ptr<T> find_first_resource_from_asset(
            const resource_registry& resourceRegistry, const asset_registry& assetRegistry, uuid assetId)
        {
            buffered_array<uuid, 16> artifacts;

            if (!assetRegistry.find_asset_artifacts(assetId, artifacts))
            {
                return {};
            }

            for (auto uuid : artifacts)
            {
                artifact_meta meta;

                if (assetRegistry.find_artifact_by_id(uuid, meta) && meta.type == resource_type<T>)
                {
                    return resourceRegistry.get_resource(uuid).as<T>();
                }
            }

            return {};
        }
    }

    TEST(gltf_importer, box)
    {
        module_manager mm;
        mm.load<test_module>();
        ASSERT_TRUE(mm.finalize());

        resource_registry resources;

        asset_registry registry;

        const string testDir{"./test/gltf_importer_suzanne/"};
        const string assetsDir{child_path(testDir, "assets")};
        const string artifactsDir{child_path(testDir, "artifacts")};
        const string sourceFilesDir{child_path(testDir, "sourcefiles")};

        const asset_repository_descriptor assetRepositories[]{
            {
                .name = "assets"_hsv,
                .assetsDirectory = assetsDir,
                .sourcesDirectory = sourceFilesDir,
            },
        };

        clear_directory(testDir);

        ASSERT_TRUE(registry.initialize(assetRepositories, artifactsDir));

        deque<resource_type_descriptor> resourceTypes;
        fetch_scene_resource_types(resourceTypes);

        for (auto& type : resourceTypes)
        {
            resources.register_type(std::move(type));
        }

        register_gltf_importer(registry);

        resources.register_provider(registry.initialize_resource_provider());

        const string gltfSampleModels{OBLO_GLTF_SAMPLE_MODELS};

        const string files[] = {
            child_path(gltfSampleModels, "Models", "Box", "glTF-Embedded", "Box.gltf"),
            child_path(gltfSampleModels, "Models", "Box", "glTF", "Box.gltf"),
            child_path(gltfSampleModels, "Models", "Box", "glTF-Binary", "Box.glb"),
        };

        string_builder dirNameBuilder;

        for (const auto& file : files)
        {
            const auto dirName = filesystem::filename(filesystem::parent_path(file, dirNameBuilder));

            string_builder destination;
            destination.append(OBLO_ASSET_PATH("assets")).append_path(dirName);

            data_document importSettings;
            importSettings.init();
            importSettings.child_value(importSettings.get_root(),
                "generateMeshlets"_hsv,
                property_value_wrapper{false});

            const auto importResult = registry.import(file, destination.view(), "Box", std::move(importSettings));

            ASSERT_TRUE(importResult);

            while (registry.get_running_import_count() > 0)
            {
                registry.update();
            }

            resources.update();

            uuid meshId;

            asset_meta assetMeta;

            string_builder assetPath;
            assetPath.format(OBLO_ASSET_PATH("assets/{}/Box"), dirName);

            ASSERT_TRUE(registry.find_asset_by_path(assetPath, meshId, assetMeta));

            ASSERT_NE(assetMeta.mainArtifactHint, uuid{});
            ASSERT_EQ(assetMeta.typeHint, resource_type<entity_hierarchy>);

            const auto hierarchyResource = resources.get_resource(assetMeta.mainArtifactHint).as<entity_hierarchy>();
            ASSERT_TRUE(hierarchyResource);

            const auto modelResource = find_first_resource_from_asset<model>(resources, registry, meshId);
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