#include <oblo/scene/scene_editor_module.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/import/file_importer.hpp>
#include <oblo/asset/import/import_artifact.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
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

namespace oblo
{
    namespace
    {
        class copy_importer final : public file_importer
        {
        public:
            explicit copy_importer(uuid typeUuid, string artifactName) :
                m_typeUuid{typeUuid}, m_artifactName{artifactName}
            {
            }

            bool init(const import_config& config, import_preview& preview)
            {
                m_source = config.sourceFile;

                auto& n = preview.nodes.emplace_back();
                n.artifactType = m_typeUuid;
                n.name = m_artifactName;

                return true;
            }

            bool import(import_context ctx)
            {
                const std::span configs = ctx.get_import_node_configs();
                const std::span nodes = ctx.get_import_nodes();

                const auto& nodeConfig = configs[0];

                if (!nodeConfig.enabled)
                {
                    return true;
                }

                string_builder destination;
                ctx.get_output_path(nodeConfig.id, destination);

                m_artifact.id = nodeConfig.id;
                m_artifact.name = m_artifactName;
                m_artifact.path = destination.as<string>();
                m_artifact.type = m_typeUuid;

                return filesystem::copy_file(m_source, destination).value_or(false);
            }

            file_import_results get_results()
            {
                file_import_results r;
                r.artifacts = {&m_artifact, 1};
                r.sourceFiles = {&m_source, 1};
                r.mainArtifactHint = m_artifact.id;
                return r;
            }

        private:
            uuid m_typeUuid;
            string m_artifactName;
            import_artifact m_artifact;
            string m_source;
        };

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
                    { return allocate_unique<copy_importer>(asset_type<material>, "material"); },
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
                    .openEditorWindow =
                        [](editor::window_manager& windowManager, editor::window_handle parent, uuid assetId)
                    { return windowManager.create_child_window<editor::material_editor>(parent, {}, assetId); },
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

    void scene_editor_module::finalize() {}
}