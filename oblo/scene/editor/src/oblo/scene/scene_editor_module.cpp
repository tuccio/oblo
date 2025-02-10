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
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/editor/commands.hpp>
#include <oblo/scene/editor/material_editor.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/traits.hpp>

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
                    .openEditorWindow = [](editor::window_manager& windowManager, uuid assetId)
                    { return windowManager.create_window<editor::material_editor>({}, {}, assetId); },
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