#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/ast/abstract_syntax_tree.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/asset_editor.hpp>
#include <oblo/editor/services/editor_directories.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/nodes/editor/node_editor.hpp>
#include <oblo/script/assets/script_graph.hpp>
#include <oblo/script/assets/traits.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace oblo::editor
{
    namespace
    {
        void draw_ast_node(string_builder& builder, const abstract_syntax_tree& ast, h32<ast_node> node)
        {
            // If the node has children, open a tree node
            builder.clear().format("Node {}", node.value);

            const auto& nodeData = ast.get(node);

            switch (nodeData.kind)
            {
            case ast_node_kind::root:
                builder.format(" [root]");
                break;

            case ast_node_kind::function_declaration:
                builder.format(" [f_decl: {}]", nodeData.node.functionDecl.name);
                break;

            case ast_node_kind::function_body:
                builder.format(" [f_body]");
                break;

            case ast_node_kind::function_parameter:
                builder.format(" [f_param: {}]", nodeData.node.functionParameter.name);
                break;

            case ast_node_kind::function_argument:
                builder.format(" [f_arg: {}]", nodeData.node.functionArgument.name);
                break;

            case ast_node_kind::f32_constant:
                builder.format(" [f32: {}]", nodeData.node.f32.value);
                break;

            case ast_node_kind::u32_constant:
                builder.format(" [u32: {}]", nodeData.node.u32.value);
                break;

            case ast_node_kind::i32_constant:
                builder.format(" [i32: {}]", nodeData.node.i32.value);
                break;

            case ast_node_kind::string_constant:
                builder.format(" [string: \"{}\"]", nodeData.node.string.value);
                break;

            case ast_node_kind::function_call:
                builder.format(" [call: {}]", nodeData.node.functionCall.name);
                break;

            case ast_node_kind::binary_operator:
                builder.format(" [binop: {}]", u32(nodeData.node.binaryOp.op));
                break;

            case ast_node_kind::variable_declaration:
                builder.format(" [var_decl: {}]", nodeData.node.varDecl.name);
                break;

            case ast_node_kind::variable_definition:
                builder.format(" [var_def]");
                break;

            case ast_node_kind::variable_reference:
                builder.format(" [var_ref: {}]", nodeData.node.varRef.name);
                break;
            default:
                break;
            }

            auto children = ast.children(node);

            if (children.begin() != children.end())
            {
                if (ImGui::TreeNodeEx(builder.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const h32 child : children)
                    {
                        draw_ast_node(builder, ast, child);
                    }

                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TreeNodeEx(builder.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
            }
        }

        class visualize_ast_window
        {
        public:
            explicit visualize_ast_window(abstract_syntax_tree ast) : m_ast{std::move(ast)} {}

            bool update(const window_update_context&)
            {
                bool isOpen = true;

                if (ImGui::Begin("Visualize AST",
                        &isOpen,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                {

                    if (m_ast.is_initialized())
                    {
                        const h32 root = m_ast.get_root();
                        string_builder builder;
                        draw_ast_node(builder, m_ast, root);
                    }
                    else
                    {
                        ImGui::TextUnformatted("AST is empty.");
                    }
                }

                ImGui::End();

                return isOpen;
            }

        private:
            abstract_syntax_tree m_ast;
        };

        class script_graph_window
        {
        public:
            script_graph_window(asset_registry& assetRegistry, uuid assetId) :
                m_assetRegistry{assetRegistry}, m_assetId{assetId}
            {
            }

            bool init(const window_update_context& ctx)
            {
                // Since we run from a DLL, ImGui globals might not be set here
                auto* const imguiCtx = ctx.services.find<ImGuiContext>();

                if (!imguiCtx)
                {
                    return false;
                }

                ImGui::SetCurrentContext(imguiCtx);

                auto asset = m_assetRegistry.load_asset(m_assetId);

                if (!asset)
                {
                    return false;
                }

                m_asset = std::move(*asset);
                auto* const sg = m_asset.as<script_graph>();

                if (!sg)
                {
                    return false;
                }

                m_editor.init(*sg);

                return true;
            }

            bool update(const window_update_context& ctx)
            {
                bool isOpen = true;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});

                if (ImGui::Begin("Node Editor",
                        &isOpen,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar))
                {
                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

                        if (ImGui::Button(ICON_FA_FLOPPY_DISK))
                        {
                            if (!save_asset(m_assetRegistry))
                            {
                                log::error("Failed to save asset {}", m_assetId);
                            }
                        }

                        if (ImGui::Button(ICON_FA_TREE))
                        {
                            auto* const sg = m_asset.as<script_graph>();

                            if (sg)
                            {
                                abstract_syntax_tree ast;

                                if (!sg->generate_ast(ast))
                                {
                                    log::error("Failed to generate AST");
                                }

                                ctx.windowManager.create_child_window<visualize_ast_window>(ctx.windowHandle,
                                    window_flags::unique_sibling,
                                    {},
                                    std::move(ast));
                            }
                        }

                        ImGui::Separator();

                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::EndMenuBar();
                    }

                    m_editor.update();
                }

                ImGui::End();

                ImGui::PopStyleVar();

                return isOpen;
            }

            expected<> save_asset(asset_registry& reg) const
            {
                return reg.save_asset(m_asset, m_assetId);
            }

        private:
            asset_registry& m_assetRegistry;
            uuid m_assetId;
            any_asset m_asset;
            node_editor m_editor;
        };

        class script_editor final : public asset_editor
        {
        public:
            expected<> open(
                window_manager& wm, asset_registry& assetRegistry, window_handle parent, uuid assetId) override
            {
                m_window = wm.create_child_window<script_graph_window>(parent, {}, {}, assetRegistry, assetId);

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
                if (auto* const w = wm.try_access<script_graph_window>(m_window))
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

        class script_asset_editor_provider final : public asset_editor_provider
        {
            void fetch(deque<asset_editor_descriptor>& out) const override
            {
                out.push_back(asset_editor_descriptor{.assetType = asset_type<script_graph>,
                    .category = "Script",
                    .name = "Script Graph",
                    .createEditor = []() -> unique_ptr<asset_editor> { return allocate_unique<script_editor>(); }});
            }
        };
    }

    class script_editor_module final : public module_interface
    {
    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        bool finalize() override;
    };

    bool script_editor_module::startup(const module_initializer& initializer)
    {
        initializer.services->add<script_asset_editor_provider>().as<asset_editor_provider>().unique();

        return true;
    }

    void script_editor_module::shutdown() {}

    bool script_editor_module::finalize()
    {
        return true;
    }
}

OBLO_MODULE_REGISTER(oblo::editor::script_editor_module)