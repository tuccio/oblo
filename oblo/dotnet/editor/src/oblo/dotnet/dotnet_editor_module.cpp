#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import/copy_importer.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/platform/shell.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/dotnet/assets/dotnet_script_asset.hpp>
#include <oblo/dotnet/utility/dotnet_utility.hpp>
#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/asset_editor.hpp>
#include <oblo/editor/services/editor_directories.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>

namespace oblo::editor
{
    namespace
    {
        class dotnet_script_editor_window
        {
        public:
            dotnet_script_editor_window(asset_registry& assetRegistry, uuid assetId) :
                m_assetRegistry{assetRegistry}, m_assetId{assetId}
            {
            }

            bool init([[maybe_unused]] const window_update_context& ctx)
            {

#if 0
                string_builder sourcePath;

                if (!m_assetRegistry.get_source_path(m_assetId, sourcePath))
                {
                    return false;
                }

                auto* editorDir = ctx.services.find<editor_directories>();

                if (!editorDir)
                {
                    return false;
                }

                string_builder workDir;

                if (!editorDir->create_temporary_directory(workDir))
                {
                    return false;
                }

                string_builder sourceFile = workDir;
                sourceFile.append_path("Script.cs");

                if (!filesystem::create_hard_link(sourcePath.as<string_view>(), sourceFile.as<string_view>()))
                {
                    return false;
                }
#else
                string_builder csproj;

                if (!m_assetRegistry.get_source_directory(m_assetId, csproj))
                {
                    return false;
                }

                csproj.append_path(".ScriptProject.csproj");

                if (!dotnet_utility::generate_csproj(csproj))
                {
                    return false;
                }

                platform::open_file(csproj);
#endif

                return true;
            }

            bool update(const window_update_context&)
            {
                return false;
            }

            expected<> save_asset(asset_registry&) const
            {
                return unspecified_error;
            }

        private:
            asset_registry& m_assetRegistry;
            uuid m_assetId;
        };

        class dotnet_script_editor final : public asset_editor
        {
        public:
            expected<> open(
                window_manager& wm, asset_registry& assetRegistry, window_handle parent, uuid assetId) override
            {
                m_window = wm.create_child_window<dotnet_script_editor_window>(parent, {}, {}, assetRegistry, assetId);

                if (!m_window)
                {
                    return unspecified_error;
                }

                return no_error;
            }

            void close(window_manager& wm) override
            {
                wm.destroy_window(m_window);
                m_window = {};
            }

            expected<> save(window_manager& wm, asset_registry& assetRegistry) override
            {
                if (auto* const w = wm.try_access<dotnet_script_editor_window>(m_window))
                {
                    return w->save_asset(assetRegistry);
                }

                return unspecified_error;
            }

            window_handle get_window() const override
            {
                return m_window;
            }

        private:
            window_handle m_window{};
        };

        class dotnet_asset_editor_provider final : public asset_editor_provider
        {
            void fetch(deque<asset_editor_descriptor>& out) const override
            {
                out.push_back(asset_editor_descriptor{.assetType = asset_type<dotnet_script_asset>,
                    .category = "Script",
                    .name = "C# Script",
                    .create =
                        []
                    {
                        dotnet_script_asset s;

                        s.code().append(R"(public class Behaviour : Oblo.IBehaviour
{
    public void OnUpdate()
    {
    }
})");

                        return any_asset{std::move(s)};
                    },
                    .createEditor = []() -> unique_ptr<asset_editor>
                    { return allocate_unique<dotnet_script_editor>(); }});
            }
        };
    }

    class dotnet_editor_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        bool finalize() override;
    };

    bool dotnet_editor_module::startup(const module_initializer& initializer)
    {
        initializer.services->add<dotnet_asset_editor_provider>().as<asset_editor_provider>().unique();

        return true;
    }

    void dotnet_editor_module::shutdown() {}

    bool dotnet_editor_module::finalize()
    {
        return true;
    }
}

OBLO_MODULE_REGISTER(oblo::editor::dotnet_editor_module)