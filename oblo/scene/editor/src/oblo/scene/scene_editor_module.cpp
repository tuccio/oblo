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
                        []
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
                        [](any_asset& asset, cstring_view source)
                    {
                        auto& m = asset.emplace<material>();
                        return m.load(source);
                    },
                    .save =
                        [](const any_asset& asset, cstring_view destination, cstring_view)
                    {
                        auto* const m = asset.as<material>();

                        if (!m)
                        {
                            return false;
                        }

                        return m->save(destination);
                    },
                    .createImporter = []() -> unique_ptr<file_importer>
                    { return allocate_unique<copy_importer>(resource_type<material>, "material"); },
                });

                out.push_back({
                    .typeUuid = asset_type<scene>,
                    .typeId = get_type_id<scene>(),
                    .fileExtension = ".oscene",
                    .create =
                        []
                    {
                        any_asset r;
                        auto& s = r.emplace<scene>();

                        if (!s.init())
                        {
                            r.clear();
                        }

                        return r;
                    },
                    .load =
                        [](any_asset& asset, cstring_view source)
                    {
                        auto& m = asset.emplace<scene>();
                        return m.load(source, {}).has_value();
                    },
                    .save =
                        [](const any_asset& asset, cstring_view destination, cstring_view)
                    {
                        auto* const m = asset.as<scene>();

                        if (!m)
                        {
                            return false;
                        }

                        return m->save(destination).has_value();
                    },
                    .createImporter = []() -> unique_ptr<file_importer>
                    { return allocate_unique<copy_importer>(resource_type<entity_hierarchy>, "scene"); },
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