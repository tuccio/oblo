#ifdef OBLO_SMOKE_DOTNET_TESTS

    #include <oblo/smoke/framework.hpp>

    #include <oblo/asset/any_asset.hpp>
    #include <oblo/asset/asset_meta.hpp>
    #include <oblo/asset/asset_registry.hpp>
    #include <oblo/core/filesystem/file.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/formatters/uuid_formatter.hpp>
    #include <oblo/core/iterator/enum_range.hpp>
    #include <oblo/dotnet/assets/dotnet_script_asset.hpp>
    #include <oblo/dotnet/components/dotnet_behaviour_component.hpp>
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
    class dotnet_entities_test final : public test
    {
    public:
        test_task run(const test_context& ctx) override
        {
            auto& assetRegistry = ctx.get_asset_registry();
            auto& resourceRegistry = ctx.get_resource_registry();

            constexpr cstring_view sourceFile = OBLO_GLTF_SAMPLE_MODELS "/Models/Duck/glTF-Embedded/Duck.gltf";

            const auto modelAssetId = assetRegistry.import(sourceFile, "$assets/", "Duck", {});
            OBLO_SMOKE_TRUE(modelAssetId);

            co_await wait_for_asset_processing(ctx, assetRegistry);

            const auto duckModel =
                find_first_resource_from_asset<model>(resourceRegistry, assetRegistry, *modelAssetId);
            OBLO_SMOKE_TRUE(duckModel);

            duckModel.load_sync();

            OBLO_SMOKE_EQ(duckModel->materials.size(), 1);
            OBLO_SMOKE_EQ(duckModel->meshes.size(), 1);

            const auto behaviourAssetId = import_script(assetRegistry, duckModel);
            OBLO_SMOKE_TRUE(behaviourAssetId);

            co_await wait_for_asset_processing(ctx, assetRegistry);

            const auto duckBehaviour =
                find_first_resource_from_asset<dotnet_assembly>(resourceRegistry, assetRegistry, *behaviourAssetId);
            OBLO_SMOKE_TRUE(duckBehaviour);

            auto& entityRegistry = ctx.get_entity_registry();

            const auto e = ecs_utility::create_named_physical_entity<dotnet_behaviour_component>(entityRegistry,
                "Script",
                {},
                vec3::splat(0),
                quaternion::identity(),
                vec3::splat(1));

            entityRegistry.get<dotnet_behaviour_component>(e).script = duckBehaviour.as_ref();

            for (u32 i = 0; i < 120;)
            {
                if (duckBehaviour.is_loaded())
                {
                    ++i;
                }

                co_await ctx.next_frame();
            }
        }

        static expected<uuid> import_script(asset_registry& assetRegistry, resource_ptr<model> model)
        {
            dotnet_script_asset asset;

            string_builder& codeBuilder = asset.scripts["DuckBehaviour.cs"];

            codeBuilder.reserve(4096);

            char meshUuid[36];
            char materialUuid[36];

            codeBuilder.format(R"(
using Oblo;
using Oblo.Behaviour;
using Oblo.Ecs;
using System.Numerics;

public class TotallyNormalDuckBehaviour : IBehaviour
{{
    const string _name = "Duck";
    const string _meshUuid = "{0}";
    const string _materialUuid = "{1}";

    private Entity _duck;

    private TimeSpan _time = TimeSpan.Zero;

    public void OnUpdate(IUpdateContext ctx)
    {{
        if (!_duck.IsAlive)
        {{
            _duck = ctx.EntityRegistry.CreateNamedPhysicalEntity(_name, EntityId.Invalid, Vector3.Zero, Quaternion.Identity, new Vector3(.01f));

            var meshComponent = _duck.AddComponent<StaticMeshComponent>();

            meshComponent.Mesh = new ResourceRef<Mesh>(Uuid.Parse(_meshUuid));
            meshComponent.Material = new ResourceRef<Material>(Uuid.Parse(_materialUuid));

            var rotationComponent = _duck.GetComponent<RotationComponent>();
            rotationComponent.Value = Quaternion.CreateFromYawPitchRoll(-MathF.PI * .5f, 0, 0);
        }}

        var positionComponent = _duck.GetComponent<PositionComponent>();

        Vector3 position = positionComponent.Value;

        float y = .25f * MathF.Sin((float)_time.TotalSeconds * 3);
        float z = -5f  + MathF.Sin((float)_time.TotalSeconds);

        positionComponent.Value = position with {{ Y = y, Z = z }};

        _time += ctx.DeltaTime;
    }}
}}
)",
                model->meshes.front().id.format_to(meshUuid),
                model->materials.front().id.format_to(materialUuid));

            return assetRegistry.create_asset(any_asset{std::move(asset)}, "$assets/", "DuckBehaviour");
        }
    };

    OBLO_SMOKE_TEST(dotnet_entities_test)
}

#endif