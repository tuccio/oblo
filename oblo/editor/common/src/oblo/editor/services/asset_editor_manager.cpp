#include <oblo/editor/services/asset_editor_manager.hpp>

#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/providers/resource_viewer_provider.hpp>
#include <oblo/modules/module_manager.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        template <typename T>
        struct destroy_subscription
        {
            ~destroy_subscription()
            {
                if (editors)
                {
                    [[maybe_unused]] const auto count = editors->erase(id);
                    OBLO_ASSERT(count > 0);
                }

                if (uniqueEditors)
                {
                    [[maybe_unused]] const auto count = uniqueEditors->erase(assetType);
                    OBLO_ASSERT(count > 0);
                }
            }

            bool update(const window_update_context&) const
            {
                return true;
            }

            uuid id{};
            uuid assetType{};
            std::unordered_map<uuid, unique_ptr<T>>* editors{};
            std::unordered_map<uuid, uuid>* uniqueEditors{};
        };

        using asset_destroy_subscription = destroy_subscription<asset_editor>;
        using resource_destroy_subscription = destroy_subscription<resource_viewer>;

        struct replace_unique_editor
        {
            bool update(const window_update_context& ctx) const
            {
                bool isOpen = true;
                bool save = false;
                bool cancel = false;

                constexpr auto popupName = "Save and close?##save_and_close";

                ImGui::OpenPopup(popupName);

                if (ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_NoSavedSettings))
                {
                    ImGui::TextUnformatted("An asset is being closed, do you wish to save it before closing?");

                    if (ImGui::Button("Save"))
                    {
                        save = true;
                        cancel = false;
                        isOpen = false;
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Discard"))
                    {
                        save = false;
                        cancel = false;
                        isOpen = false;
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel"))
                    {
                        save = false;
                        cancel = true;
                        isOpen = false;
                    }

                    ImGui::SameLine();

                    ImGui::EndPopup();
                }

                if (!isOpen)
                {
                    const auto oldAssetId = assetEditorManager->find_unique_asset_editor(descriptor.assetType);

                    if (save)
                    {
                        if (!oldAssetId.is_nil())
                        {
                            assetEditorManager->save_asset(ctx.windowManager, oldAssetId).assert_value();
                        }
                    }

                    if (!cancel)
                    {
                        if (!oldAssetId.is_nil())
                        {
                            assetEditorManager->close_asset(ctx.windowManager, oldAssetId);
                        }

                        assetEditorManager->open_asset(ctx.windowManager, assetId, descriptor.assetType).assert_value();
                    }
                }

                return isOpen;
            }

            asset_editor_manager* assetEditorManager{};
            asset_editor_descriptor descriptor{};
            uuid assetId{};
        };
    }

    asset_editor_manager::~asset_editor_manager() = default;

    asset_editor_manager::asset_editor_manager(asset_registry& assetRegistry,
        const resource_registry& resourceRegistry) :
        m_assetRegistry{assetRegistry}, m_resourceRegistry{resourceRegistry}
    {
        auto& mm = module_manager::get();

        deque<asset_editor_descriptor> editorDescs;

        for (auto* const createProvider : mm.find_services<asset_editor_provider>())
        {
            editorDescs.clear();
            createProvider->fetch(editorDescs);

            m_editorDescriptors.append(editorDescs.begin(), editorDescs.end());
        }

        deque<resource_viewer_descriptor> viewerDescs;

        for (auto* const createProvider : mm.find_services<resource_viewer_provider>())
        {
            viewerDescs.clear();
            createProvider->fetch(viewerDescs);

            m_viewerDescriptors.append(viewerDescs.begin(), viewerDescs.end());
        }
    }

    void asset_editor_manager::set_window_root(window_handle root)
    {
        m_root = root;
    }

    expected<success_tag, asset_editor_manager::open_error> asset_editor_manager::open_asset(
        window_manager& wm, const uuid& assetId, const uuid& assetType)
    {
        const auto [it, inserted] = m_editors.emplace(assetId, unique_ptr<asset_editor>{});

        if (!inserted)
        {
            return open_error::already_open;
        }

        for (const auto& desc : m_editorDescriptors)
        {
            if (desc.assetType == assetType)
            {
                if (!desc.createEditor)
                {
                    continue;
                }

                if (desc.flags.contains(editor::asset_editor_flags::unique_type))
                {
                    const auto uIt = m_uniqueEditors.find(assetType);

                    if (uIt != m_uniqueEditors.end())
                    {
                        const auto replace = wm.create_window<replace_unique_editor>({}, {});
                        auto* const ptr = wm.try_access<replace_unique_editor>(replace);
                        OBLO_ASSERT(ptr);

                        if (ptr)
                        {
                            ptr->assetEditorManager = this;
                            ptr->descriptor = desc;
                            ptr->assetId = assetId;
                        }

                        // The replacement will open the editor later if the user goes on with it
                        m_editors.erase(it);

                        return no_error;
                    }
                }

                unique_ptr assetEditor = desc.createEditor();

                if (assetEditor && assetEditor->open(wm, m_assetRegistry, m_root, assetId))
                {
                    const auto root = assetEditor->get_window();

                    const auto subscription = wm.create_child_window<asset_destroy_subscription>(root);
                    auto* const ptr = wm.try_access<asset_destroy_subscription>(subscription);
                    OBLO_ASSERT(ptr);

                    if (ptr)
                    {
                        ptr->id = assetId;
                        ptr->editors = &m_editors;
                    }

                    if (desc.flags.contains(editor::asset_editor_flags::unique_type))
                    {
                        m_uniqueEditors.emplace(assetType, assetId);

                        ptr->assetType = assetType;
                        ptr->uniqueEditors = &m_uniqueEditors;
                    }

                    it->second = std::move(assetEditor);

                    return no_error;
                }
                else
                {
                    m_editors.erase(it);
                    return open_error::unspecified_error;
                }
            }
        }

        m_editors.erase(it);

        return open_error::no_such_type;
    }

    expected<success_tag, asset_editor_manager::open_error> asset_editor_manager::open_resource(
        window_manager& wm, const uuid& resourceId, const uuid& resourceType)
    {
        const auto [it, inserted] = m_viewers.emplace(resourceId, unique_ptr<resource_viewer>{});

        if (!inserted)
        {
            return open_error::already_open;
        }

        for (const auto& desc : m_viewerDescriptors)
        {
            if (desc.resourceType == resourceType)
            {
                if (!desc.createViewer)
                {
                    continue;
                }

                unique_ptr resourceViewer = desc.createViewer();

                if (resourceViewer && resourceViewer->open(wm, m_resourceRegistry, m_root, resourceId))
                {
                    const auto root = resourceViewer->get_window();

                    const auto subscription = wm.create_child_window<resource_destroy_subscription>(root);
                    auto* const ptr = wm.try_access<resource_destroy_subscription>(subscription);
                    OBLO_ASSERT(ptr);

                    if (ptr)
                    {
                        ptr->id = resourceId;
                        ptr->editors = &m_viewers;
                    }

                    it->second = std::move(resourceViewer);

                    return no_error;
                }
                else
                {
                    m_viewers.erase(it);
                    return open_error::unspecified_error;
                }
            }
        }

        m_viewers.erase(it);

        return open_error::no_such_type;
    }

    uuid asset_editor_manager::find_unique_asset_editor(const uuid& assetType)
    {
        const auto it = m_uniqueEditors.find(assetType);

        if (it == m_uniqueEditors.end())
        {
            return uuid{};
        }

        return it->second;
    }

    void asset_editor_manager::close_asset(window_manager& wm, const uuid& assetId)
    {
        const auto it = m_editors.find(assetId);

        if (it == m_editors.end())
        {
            return;
        }

        it->second->close(wm);
    }

    void asset_editor_manager::close_resource(window_manager& wm, const uuid& resourceId)
    {
        const auto it = m_viewers.find(resourceId);

        if (it == m_viewers.end())
        {
            return;
        }

        it->second->close(wm);
    }

    expected<> asset_editor_manager::save_asset(window_manager& wm, const uuid& assetId)
    {
        const auto it = m_editors.find(assetId);

        if (it == m_editors.end())
        {
            return "Editor operation failed"_err;
        }

        return it->second->save(wm, m_assetRegistry);
    }

    window_handle asset_editor_manager::get_asset_window(const uuid& assetId) const
    {
        const auto it = m_editors.find(assetId);

        if (it == m_editors.end())
        {
            return {};
        }

        return it->second->get_window();
    }

    window_handle oblo::editor::asset_editor_manager::get_resource_window(const uuid& resourceId) const
    {
        const auto it = m_viewers.find(resourceId);

        if (it == m_viewers.end())
        {
            return {};
        }

        return it->second->get_window();
    }
}