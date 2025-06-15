#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/providers/native_asset_provider.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/providers/service_provider.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/asset_editor.hpp>
#include <oblo/editor/services/editor_directories.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/nodes/editor/node_editor.hpp>
#include <oblo/script/assets/script_graph.hpp>
#include <oblo/script/assets/traits.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
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

                return true;
            }

            bool update(const window_update_context&)
            {
                bool isOpen = true;

                if (ImGui::Begin("Node Editor", &isOpen))
                {
                    m_editor.update();
                }

                ImGui::End();

                return isOpen;
            }

            expected<> save_asset(asset_registry&) const
            {
                return unspecified_error;
            }

        private:
            asset_registry& m_assetRegistry;
            uuid m_assetId;
            nodes::node_editor m_editor;
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