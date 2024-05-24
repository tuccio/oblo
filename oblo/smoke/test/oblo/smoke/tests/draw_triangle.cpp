#include <oblo/smoke/framework.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include <gtest/gtest.h>

namespace oblo::smoke
{
    namespace
    {
        template <typename T>
        resource_ptr<T> import_as_resource(
            asset_registry& assetRegistry, resource_registry& resourceRegistry, const std::filesystem::path& source)
        {
            importer assetImporter = assetRegistry.create_importer(source);

            if (!assetImporter.is_valid() || !assetImporter.init() || !assetImporter.execute("."))
            {
                return {};
            }

            const uuid assetUuid = assetImporter.get_import_id();

            asset_meta assetMeta;
            assetRegistry.find_asset_by_id(assetUuid, assetMeta);

            if (assetMeta.typeHint != get_type_id<T>())
            {
                return {};
            }

            return resourceRegistry.get_resource(assetMeta.mainArtifactHint).as<T>();
        }
    }

    class draw_triangle final : public test
    {
    public:
        test_task run(const test_context& ctx) override
        {
            auto& assetRegistry = ctx.get_asset_registry();
            auto& resourceRegistry = ctx.get_resource_registry();

            const auto triangle = import_as_resource<model>(assetRegistry,
                resourceRegistry,
                OBLO_GLTF_SAMPLE_MODELS "/Models/SimpleMaterial/glTF-Embedded/SimpleMaterial.gltf");

            OBLO_SMOKE_TRUE(triangle);
            OBLO_SMOKE_EQ(triangle->materials.size(), 1);
            OBLO_SMOKE_EQ(triangle->meshes.size(), 1);

            auto& entities = ctx.get_entity_registry();

            const auto triangleEntity = ecs_utility::create_named_physical_entity<static_mesh_component>(entities,
                "triangle",
                vec3{.z = -2.f},
                quaternion::identity(),
                vec3::splat(1.f));

            auto& mesh = entities.get<static_mesh_component>(triangleEntity);
            mesh.material = triangle->materials[0];
            mesh.mesh = triangle->meshes[0];

            co_await ctx.next_frame();
        }
    };

    OBLO_SMOKE_TEST(draw_triangle)

    class crash_repro final : public test
    {
    public:
        test_task run(const test_context& ctx) override
        {
            auto& assetRegistry = ctx.get_asset_registry();
            auto& resourceRegistry = ctx.get_resource_registry();

            const auto triangle = import_as_resource<model>(assetRegistry,
                resourceRegistry,
                OBLO_GLTF_SAMPLE_MODELS "/Models/SimpleMaterial/glTF-Embedded/SimpleMaterial.gltf");

            OBLO_SMOKE_TRUE(triangle);
            OBLO_SMOKE_EQ(triangle->materials.size(), 1);
            OBLO_SMOKE_EQ(triangle->meshes.size(), 1);

            auto& entities = ctx.get_entity_registry();

            ecs::entity triangles[2]{};

            for (u32 i = 0; i < std::size(triangles); ++i)
            {
                const auto triangleEntity = ecs_utility::create_named_physical_entity<static_mesh_component>(entities,
                    "triangle",
                    vec3{.x = -1.f + i * 1.f, .z = -2.f},
                    quaternion::identity(),
                    vec3::splat(1.f));

                auto& mesh = entities.get<static_mesh_component>(triangleEntity);
                mesh.material = triangle->materials[0];
                mesh.mesh = triangle->meshes[0];

                triangles[i] = triangleEntity;
            }

            // ctx.request_renderdoc_capture();

            co_await ctx.next_frame();

            // TODO: THIS IS MESSING UP FRAME 0!!
            entities.remove<scale_component>(triangles[0]);
            co_await ctx.next_frame();

            // ctx.request_renderdoc_capture();

            // co_await ctx.next_frame();
        }
    };

    OBLO_SMOKE_TEST(crash_repro)
}
