#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/import/copy_importer.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/platform/file.hpp>
#include <oblo/core/platform/process.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/nodes/common/binary_operators.hpp>
#include <oblo/nodes/common/constant_nodes.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/common/input_node.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph_registry.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/tags/ecs.hpp>
#include <oblo/runtime/runtime_module.hpp>
#include <oblo/script/assets/script_graph.hpp>
#include <oblo/script/assets/traits.hpp>
#include <oblo/script/compiler/bytecode_generator.hpp>
#include <oblo/script/nodes/ecs_nodes.hpp>
#include <oblo/script/resources/compiled_script.hpp>
#include <oblo/script/resources/traits.hpp>

namespace oblo
{
    namespace
    {
        bool load_graph(script_graph& g, const node_graph_registry& reg, cstring_view source)
        {
            g.init(reg);

            data_document doc;

            if (!json::read(doc, source) || !g.deserialize(doc, doc.get_root()))
            {
                return false;
            }

            return true;
        }

        class script_graph_importer : public file_importer
        {
            static constexpr cstring_view g_ArtifactName = "script.obytecode";

        public:
            explicit script_graph_importer(const node_graph_registry& registry) : m_registry{registry} {}

            bool init(const import_config& config, import_preview& preview)
            {
                m_source = config.sourceFile;

                auto& n = preview.nodes.emplace_back();
                n.artifactType = resource_type<compiled_script>;
                n.name = g_ArtifactName;

                return true;
            }

            bool import(import_context ctx)
            {
                const std::span configs = ctx.get_import_node_configs();

                const auto& nodeConfig = configs[0];

                if (!nodeConfig.enabled)
                {
                    return true;
                }

                string_builder destination;
                ctx.get_output_path(nodeConfig.id, destination);

                if (!filesystem::create_directories(destination))
                {
                    return false;
                }

                destination.append_path(g_ArtifactName);

                script_graph sg;

                if (!load_graph(sg, m_registry, m_source))
                {
                    return false;
                }

                abstract_syntax_tree ast;

                if (!sg.generate_ast(ast))
                {
                    return false;
                }

                auto module = bytecode_generator{}.generate_module(ast);

                if (!module)
                {
                    return false;
                }

                compiled_script script;
                script.module = std::move(*module);

                if (!save(script, destination))
                {
                    return false;
                }

                m_sourceFiles.emplace_back(m_source);

                m_artifact.id = nodeConfig.id;
                m_artifact.name = g_ArtifactName;
                m_artifact.path = destination.as<string>();
                m_artifact.type = resource_type<compiled_script>;

                return true;
            }

            file_import_results get_results()
            {
                file_import_results r;
                r.artifacts = {&m_artifact, 1};
                r.sourceFiles = m_sourceFiles;
                r.mainArtifactHint = m_artifact.id;
                return r;
            }

        private:
            const node_graph_registry& m_registry{};
            import_artifact m_artifact{};
            string m_source;
            dynamic_array<string> m_sourceFiles;
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
                        [](any_asset& asset, cstring_view source, const any&)
                    {
                        auto* const self = module_manager::get().find_unique_service<script_graph_provider>();

                        if (!self)
                        {
                            return false;
                        }

                        script_graph g;

                        if (!load_graph(g, self->m_registry, source))
                        {
                            return false;
                        }

                        asset.emplace<script_graph>(std::move(g));
                        return true;
                    },
                    .save =
                        [](const any_asset& asset, cstring_view destination, cstring_view, const any&)
                    {
                        auto* const g = asset.as<script_graph>();

                        if (!g)
                        {
                            return false;
                        }

                        data_document doc;
                        doc.init();

                        if (!g->serialize(doc, doc.get_root()))
                        {
                            return false;
                        }

                        return json::write(doc, destination).has_value();
                    },
                    .createImporter = [](const any& userdata) -> unique_ptr<file_importer>
                    {
                        auto* const* const reg = userdata.as<const node_graph_registry*>();
                        return allocate_unique<script_graph_importer>(**reg);
                    },
                    .userdata = make_any<const node_graph_registry*>(&m_registry),
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
                .instantiate = [](const any&) -> unique_ptr<node_interface> { return allocate_unique<T>(); },
            };
        }
    }

    class script_asset_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override
        {
            initializer.services->add<script_graph_provider>().as<native_asset_provider>().unique(m_scriptRegistry);

            // Load the runtime module to make sure it gets finalized before us, since we want to register the
            // components properties from its property registry
            module_manager::get().load<runtime_module>();

            return true;
        }

        bool finalize() override
        {
            // Types
            m_scriptRegistry.register_primitive_type(make_node_primitive_type<node_primitive_kind::boolean>());
            m_scriptRegistry.register_primitive_type(make_node_primitive_type<node_primitive_kind::i32>());
            m_scriptRegistry.register_primitive_type(make_node_primitive_type<node_primitive_kind::f32>());

            // Nodes
            m_scriptRegistry.register_node(make_node_descriptor<input_node>());

            m_scriptRegistry.register_node(make_node_descriptor<bool_constant_node>());
            m_scriptRegistry.register_node(make_node_descriptor<i32_constant_node>());
            m_scriptRegistry.register_node(make_node_descriptor<f32_constant_node>());

            m_scriptRegistry.register_node(make_node_descriptor<add_operator>());
            m_scriptRegistry.register_node(make_node_descriptor<mul_operator>());

            auto* const runtimeModule = module_manager::get().find<runtime_module>();

            if (runtimeModule)
            {
                const auto& propertyRegistry = runtimeModule->get_property_registry();
                const auto& reflectionRegistry = propertyRegistry.get_reflection_registry();

                deque<reflection::type_handle> componentTypes;
                reflectionRegistry.find_by_tag<ecs::component_type_tag>(componentTypes);

                const uuid_namespace_generator getPropertyIdGen{"598e9195-326b-4ab8-a397-004d85a9c036"_uuid};
                const uuid_namespace_generator setPropertyIdGen{"b05cf6fe-ecbb-4699-9a8f-ad48a7889086"_uuid};

                string_builder nodeName;
                string_builder propertyPath;

                for (const auto& componentType : componentTypes)
                {
                    const auto& typeData = reflectionRegistry.get_type_data(componentType);

                    auto* const propertyTree = propertyRegistry.try_get(typeData.type);

                    if (propertyTree)
                    {
                        for (auto& property : propertyTree->properties)
                        {
                            struct ecs_property_userdata
                            {
                                type_id componentType;
                                string propertyPath;
                                uuid nodeGraphType;
                            };

                            uuid nodeGraphType{};

                            switch (property.kind)
                            {
                            case property_kind::boolean:
                                nodeGraphType = get_node_primitive_type_id<node_primitive_kind::boolean>();
                                break;

                            case property_kind::f32:
                                nodeGraphType = get_node_primitive_type_id<node_primitive_kind::f32>();
                                break;

                            case property_kind::i32:
                                nodeGraphType = get_node_primitive_type_id<node_primitive_kind::i32>();
                                break;

                            default:
                                break;
                            }

                            if (nodeGraphType.is_nil())
                            {
                                continue;
                            }

                            propertyPath.clear();
                            create_property_path(propertyPath, *propertyTree, property);

                            const ecs_property_userdata userdata{
                                .componentType = typeData.type,
                                .propertyPath = propertyPath.as<string>(),
                                .nodeGraphType = nodeGraphType,
                            };

                            nodeName.clear()
                                .append("Get ")
                                .append(typeData.type.name)
                                .append("::")
                                .append(propertyPath);

                            m_scriptRegistry.register_node({
                                .id = getPropertyIdGen.generate(propertyPath.view()),
                                .name = nodeName.as<string>(),
                                .instantiate = [](const any& userdata) -> unique_ptr<node_interface>
                                {
                                    auto* const propertyUserdata = userdata.as<ecs_property_userdata>();

                                    return allocate_unique<ecs_nodes::get_component_property_node>(
                                        propertyUserdata->componentType,
                                        propertyUserdata->propertyPath,
                                        propertyUserdata->nodeGraphType);
                                },
                                .userdata = make_any<ecs_property_userdata>(userdata),
                            });

                            nodeName.clear()
                                .append("Set ")
                                .append(typeData.type.name)
                                .append("::")
                                .append(propertyPath);

                            m_scriptRegistry.register_node({
                                .id = setPropertyIdGen.generate(propertyPath.view()),
                                .name = nodeName.as<string>(),
                                .instantiate = [](const any& userdata) -> unique_ptr<node_interface>
                                {
                                    auto* const propertyUserdata = userdata.as<ecs_property_userdata>();

                                    return allocate_unique<ecs_nodes::set_component_property_node>(
                                        propertyUserdata->componentType,
                                        propertyUserdata->propertyPath,
                                        propertyUserdata->nodeGraphType);
                                },
                                .userdata = make_any<ecs_property_userdata>(userdata),
                            });
                        }
                    }
                }
            }

            return true;
        }

        void shutdown() override {}

    private:
        node_graph_registry m_scriptRegistry;
    };
}

OBLO_MODULE_REGISTER(oblo::script_asset_module)