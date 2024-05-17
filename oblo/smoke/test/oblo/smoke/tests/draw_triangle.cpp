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
#include <oblo/scene/utility/ecs_utility.hpp>

#include <gtest/gtest.h>

namespace oblo::smoke
{
    class draw_triangle final : public test
    {
    public:
        test_task run(const test_context& ctx) override
        {
            auto& assetRegistry = ctx.get_asset_registry();

            constexpr auto triangleSource = OBLO_GLTF_SAMPLE_MODELS "/Models/Triangle/glTF/Triangle.gltf";

            auto triangleImporter = assetRegistry.create_importer(triangleSource);

            OBLO_SMOKE_TRUE(triangleImporter.is_valid());

            OBLO_SMOKE_TRUE(triangleImporter.init());
            OBLO_SMOKE_TRUE(triangleImporter.execute("."));

            uuid assetUuid;
            asset_meta assetMeta;
            OBLO_SMOKE_TRUE(assetRegistry.find_asset_by_path("./Triangle", assetUuid, assetMeta));

            auto& resourceRegistry = ctx.get_resource_registry();

            OBLO_SMOKE_EQ(assetMeta.typeHint, get_type_id<model>());

            const auto modelResource = resourceRegistry.get_resource(assetMeta.mainArtifactHint).as<model>();
            OBLO_SMOKE_TRUE(modelResource);

            OBLO_SMOKE_EQ(modelResource->materials.size(), 1);
            OBLO_SMOKE_EQ(modelResource->meshes.size(), 1);

            auto& entities = ctx.get_entity_registry();

            const auto triangleEntity = ecs_utility::create_named_physical_entity<static_mesh_component>(entities,
                "triangle",
                vec3{},
                quaternion::identity(),
                vec3::splat(1.f));

            auto& mesh = entities.get<static_mesh_component>(triangleEntity);
            mesh.material = modelResource->materials[0];
            mesh.mesh = modelResource->meshes[0];

            co_await ctx.next_frame();

            while (true)
            {
                co_await ctx.next_frame();
            }
        }
    };

    OBLO_SMOKE_TEST(draw_triangle)
}
