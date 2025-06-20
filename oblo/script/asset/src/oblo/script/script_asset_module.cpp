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
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/nodes/binary_operators.hpp>
#include <oblo/nodes/constant_nodes.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph_registry.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/script/assets/script_graph.hpp>
#include <oblo/script/assets/traits.hpp>

namespace oblo
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
        public:
            script_graph_provider(const node_graph_registry& registry) : m_registry{registry} {}

            void fetch(deque<native_asset_descriptor>& out) const override
            {
                out.push_back({
                    .typeUuid = asset_type<script_graph>,
                    .typeId = get_type_id<script_graph>(),
                    .fileExtension = ".osgraph",
                    .create =
                        [](const any&)
                    {
                        auto* const self = module_manager::get().find_unique_service<script_graph_provider>();

                        if (!self)
                        {
                            return any_asset{};
                        }

                        script_graph g;
                        g.init(self->m_registry);

                        return any_asset{std::move(g)};
                    },
                    .load =
                        [](any_asset& asset, cstring_view, const any&)
                    {
                        auto* const self = module_manager::get().find_unique_service<script_graph_provider>();

                        if (!self)
                        {
                            return false;
                        }

                        script_graph g;
                        g.init(self->m_registry);

                        asset.emplace<script_graph>(std::move(g));

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

        private:
            const node_graph_registry& m_registry;
        };

        template <typename T>
        node_descriptor make_node_descriptor()
        {
            return {
                .id = T::id,
                .name = string{T::name},
                .instantiate = []() -> unique_ptr<node_interface> { return allocate_unique<T>(); },
            };
        }
    }

    class script_asset_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<script_graph_provider>().as<native_asset_provider>().unique(m_scriptRegistry);
            return true;
        }

        bool finalize() override
        {
            m_scriptRegistry.register_node(make_node_descriptor<bool_constant_node>());
            m_scriptRegistry.register_node(make_node_descriptor<f32_constant_node>());

            m_scriptRegistry.register_node(make_node_descriptor<add_operator>());
            m_scriptRegistry.register_node(make_node_descriptor<mul_operator>());

            return true;
        }

        void shutdown() override {}

    private:
        node_graph_registry m_scriptRegistry;
    };
}

OBLO_MODULE_REGISTER(oblo::script_asset_module)