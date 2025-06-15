#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/import/copy_importer.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/platform/file.hpp>
#include <oblo/core/platform/process.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/script/assets/script_graph.hpp>
#include <oblo/script/assets/traits.hpp>

namespace oblo::script
{
    namespace
    {
        class dummy_importer : public file_importer
        {
        public:
            bool init(const import_config&, import_preview&) override
            {
                return true;
            }

            bool import(import_context) override
            {
                return true;
            }

            file_import_results get_results() override
            {
                return {};
            }
        };

        class script_graph_provider final : public native_asset_provider
        {
            void fetch(deque<native_asset_descriptor>& out) const override
            {
                out.push_back({
                    .typeUuid = asset_type<script_graph>,
                    .typeId = get_type_id<script_graph>(),
                    .fileExtension = ".osgraph",
                    .create =
                        [](const any&)
                    {
                        script_graph g;

                        // TODO: Init, setup nodes

                        return any_asset{std::move(g)};
                    },
                    .load =
                        [](any_asset&, cstring_view, const any&)
                    {
                        // TODO
                        return true;
                    },
                    .save =
                        [](const any_asset&, cstring_view, cstring_view, const any&)
                    {
                        return true;
                        // TODO
                    },
                    .createImporter = []() -> unique_ptr<file_importer>
                    {
                        // TODO: Actually need to build the script from the graph
                        return allocate_unique<dummy_importer>();
                    },
                });
            }
        };
    }

    class script_asset_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<script_graph_provider>().as<native_asset_provider>().unique();
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };
}

OBLO_MODULE_REGISTER(oblo::script::script_asset_module)