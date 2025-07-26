#include <oblo/scene/scene_editor_module.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/import/copy_importer.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/graphics/components/light_component.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/scene/assets/scene.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/editor/commands.hpp>
#include <oblo/scene/editor/material_editor.hpp>
#include <oblo/scene/editor/scene_editor.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

namespace oblo
{
    namespace
    {
        class scene_asset_provider final : public native_asset_provider
        {
            void fetch(deque<native_asset_descriptor>& out) const override
            {
                out.push_back({
                    .typeUuid = asset_type<material>,
                    .typeId = get_type_id<material>(),
                    .fileExtension = ".omaterial",
                    .create =
                        [](const any&)
                    {
                        material m;

                        // TODO: Set all properties
                        pbr::properties properties;

                        struct_apply([&m](auto&... descs)
                            { (m.set_property(descs.name, descs.type, descs.defaultValue), ...); },
                            properties);

                        return any_asset{std::move(m)};
                    },
                    .load =
                        [](any_asset& asset, cstring_view source, const any&)
                    {
                        auto& m = asset.emplace<material>();
                        return m.load(source);
                    },
                    .save =
                        [](const any_asset& asset, cstring_view destination, cstring_view, const any&)
                    {
                        auto* const m = asset.as<material>();

                        if (!m)
                        {
                            return false;
                        }

                        return m->save(destination);
                    },
                    .createImporter = [](const any&) -> unique_ptr<file_importer>
                    { return allocate_unique<copy_importer>(resource_type<material>, "material"); },
                });

                any sceneCtx;
                sceneCtx.emplace<entity_hierarchy_serialization_context>().init().assert_value();

                out.push_back({
                    .typeUuid = asset_type<scene>,
                    .typeId = get_type_id<scene>(),
                    .fileExtension = ".oscene",
                    .create =
                        [](const any& ctx)
                    {
                        any_asset r;
                        auto& s = r.emplace<scene>();

                        if (!s.init(ctx.as<entity_hierarchy_serialization_context>()->get_type_registry()))
                        {
                            r.clear();
                        }

                        auto& entities = s.get_entity_registry();

                        const auto e = ecs_utility::create_named_physical_entity<light_component>(entities,
                            "Sun",
                            {},
                            {},
                            quaternion::from_euler_xyz_intrinsic(degrees_tag{},
                                vec3{.x = -69.f, .y = -29.f, .z = -2.f}),
                            vec3::splat(1.f));

                        entities.get<light_component>(e) = {
                            .type = light_type::directional,
                            .color = vec3::splat(1.f),
                            .intensity = 50.f,
                            .isShadowCaster = true,
                            .hardShadows = false,
                            .shadowBias = .025f,
                            .shadowPunctualRadius = 100.f,
                            .shadowDepthSigma = 1e-2f,
                            .shadowTemporalAccumulationFactor = .98f,
                            .shadowMeanFilterSize = 17,
                            .shadowMeanFilterSigma = 1.f,
                        };

                        return r;
                    },
                    .load =
                        [](any_asset& asset, cstring_view source, const any& ctx)
                    {
                        auto& ehCtx = *ctx.as<entity_hierarchy_serialization_context>();
                        auto& m = asset.emplace<scene>();
                        return m.init(ehCtx.get_type_registry()).has_value() && m.load(source, ehCtx).has_value();
                    },
                    .save =
                        [](const any_asset& asset, cstring_view destination, cstring_view, const any& ctx)
                    {
                        auto* const m = asset.as<scene>();

                        if (!m)
                        {
                            return false;
                        }

                        return m->save(destination, *ctx.as<entity_hierarchy_serialization_context>()).has_value();
                    },
                    .createImporter = [](const any&) -> unique_ptr<file_importer>
                    { return allocate_unique<copy_importer>(resource_type<entity_hierarchy>, "scene"); },
                    .userdata = std::move(sceneCtx),
                });
            }
        };

        class scene_asset_editor_provider final : public editor::asset_editor_provider
        {
            void fetch(deque<editor::asset_editor_descriptor>& out) const override
            {
                out.push_back(editor::asset_editor_descriptor{
                    .assetType = asset_type<material>,
                    .category = "Material",
                    .name = "PBR",
                    .createEditor = []() -> unique_ptr<editor::asset_editor>
                    { return allocate_unique<editor::material_editor>(); },
                });

                out.push_back(editor::asset_editor_descriptor{
                    .assetType = asset_type<scene>,
                    .name = "Scene",
                    .createEditor = []() -> unique_ptr<editor::asset_editor>
                    { return allocate_unique<editor::scene_editor>(); },
                    .flags = editor::asset_editor_flags::unique_type,
                });
            }
        };

        class editor_service_registrant final : public editor::service_provider
        {
            void fetch(deque<editor::service_provider_descriptor>& out) const override
            {
                out.push_back(editor::service_provider_descriptor{
                    .registerServices = [](service_registry& registry) { fill_spawn_commands(registry); }});
            }
        };
    }

    bool scene_editor_module::startup(const module_initializer& initializer)
    {
        initializer.services->add<scene_asset_provider>().as<native_asset_provider>().unique();
        initializer.services->add<scene_asset_editor_provider>().as<editor::asset_editor_provider>().unique();
        initializer.services->add<editor_service_registrant>().as<editor::service_provider>().unique();

        return true;
    }

    void scene_editor_module::shutdown() {}

    bool scene_editor_module::finalize()
    {
        return true;
    }
}