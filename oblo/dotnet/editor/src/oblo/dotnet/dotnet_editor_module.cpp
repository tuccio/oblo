#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/import/copy_importer.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/dotnet/assets/dotnet_script_asset.hpp>
#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>

namespace oblo
{
    namespace
    {
        class dotnet_asset_editor_provider final : public editor::asset_editor_provider
        {
            void fetch(deque<editor::asset_editor_descriptor>& out) const override
            {
                out.push_back(editor::asset_editor_descriptor{
                    .assetType = asset_type<dotnet_script_asset>,
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
                });
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
        initializer.services->add<dotnet_asset_editor_provider>().as<editor::asset_editor_provider>().unique();

        return true;
    }

    void dotnet_editor_module::shutdown() {}

    bool dotnet_editor_module::finalize()
    {
        return true;
    }
}

OBLO_MODULE_REGISTER(oblo::dotnet_editor_module)