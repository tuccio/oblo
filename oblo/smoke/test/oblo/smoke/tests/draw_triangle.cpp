#include <oblo/smoke/framework.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_path.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/iterator/enum_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/graphics/components/camera_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/graphics/components/viewport_component.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/position_component.hpp>
#include <oblo/scene/components/rotation_component.hpp>
#include <oblo/scene/components/scale_component.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>
#include <oblo/smoke/tests/asset_utility.hpp>

#include <gtest/gtest.h>

namespace oblo::smoke
{
    class draw_triangle final : public test
    {
    public:
        test_task run(const test_context& ctx) override
        {
            auto& assetRegistry = ctx.get_asset_registry();
            auto& resourceRegistry = ctx.get_resource_registry();

            constexpr cstring_view sourceFile =
                OBLO_GLTF_SAMPLE_MODELS "/Models/SimpleMaterial/glTF-Embedded/SimpleMaterial.gltf";

            const auto assetId = assetRegistry.import(sourceFile, OBLO_ASSET_PATH("assets/"), "SimpleMaterial", {});

            OBLO_SMOKE_TRUE(assetId);

            co_await wait_for_asset_processing(ctx, assetRegistry);

            const auto triangle = find_first_resource_from_asset<model>(resourceRegistry, assetRegistry, *assetId);
            OBLO_SMOKE_TRUE(triangle);

            triangle.load_sync();

            OBLO_SMOKE_EQ(triangle->materials.size(), 1);
            OBLO_SMOKE_EQ(triangle->meshes.size(), 1);

            auto& entities = ctx.get_entity_registry();

            const auto triangleEntity = ecs_utility::create_named_physical_entity<static_mesh_component>(entities,
                "triangle",
                {},
                vec3{.z = -2.f},
                quaternion::identity(),
                vec3::splat(1.f));

            auto& mesh = entities.get<static_mesh_component>(triangleEntity);
            mesh.material = triangle->materials[0];
            mesh.mesh = triangle->meshes[0];

            co_await ctx.next_frame();

            // Switch viewport mode and render
            for (const auto mode : {
                     viewport_mode::albedo,
                     viewport_mode::normals,
                     viewport_mode::normal_map,
                     viewport_mode::uv0,
                     viewport_mode::meshlet,
                     viewport_mode::raytracing_debug,
                     viewport_mode::lit,
                 })
            {
                entities.get<viewport_component>(ctx.get_camera_entity()).mode = mode;
                entities.notify(triangleEntity);

                co_await ctx.next_frame();
            }
        }
    };

    OBLO_SMOKE_TEST(draw_triangle)

    class draw_and_remove final : public test
    {
    public:
        test_task run(const test_context& ctx) override
        {
            auto& assetRegistry = ctx.get_asset_registry();
            auto& resourceRegistry = ctx.get_resource_registry();

            constexpr cstring_view sourceFile =
                OBLO_GLTF_SAMPLE_MODELS "/Models/SimpleMaterial/glTF-Embedded/SimpleMaterial.gltf";

            const auto assetId = assetRegistry.import(sourceFile, OBLO_ASSET_PATH("assets/"), "SimpleMaterial", {});

            OBLO_SMOKE_TRUE(assetId);

            co_await wait_for_asset_processing(ctx, assetRegistry);

            const auto triangle = find_first_resource_from_asset<model>(resourceRegistry, assetRegistry, *assetId);

            OBLO_SMOKE_TRUE(triangle);
            triangle.load_sync();

            OBLO_SMOKE_EQ(triangle->materials.size(), 1);
            OBLO_SMOKE_EQ(triangle->meshes.size(), 1);

            auto& entities = ctx.get_entity_registry();

            ecs::entity triangles[2]{};

            for (u32 i = 0; i < std::size(triangles); ++i)
            {
                const auto triangleEntity = ecs_utility::create_named_physical_entity<static_mesh_component>(entities,
                    "triangle",
                    {},
                    vec3{.x = -1.f + i * 1.f, .z = -2.f},
                    quaternion::identity(),
                    vec3::splat(1.f));

                auto& mesh = entities.get<static_mesh_component>(triangleEntity);
                mesh.material = triangle->materials[0];
                mesh.mesh = triangle->meshes[0];

                triangles[i] = triangleEntity;
            }

            co_await ctx.next_frame();

            // TODO: (#30) Removing this should remove the mesh from rendering, but it does not currently
            entities.remove<static_mesh_component>(triangles[0]);

            co_await ctx.next_frame();
        }
    };

    OBLO_SMOKE_TEST(draw_and_remove)
}
