#include <oblo/editor/services/asset_editor_manager.hpp>

#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/modules/module_manager.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
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
            std::unordered_map<uuid, unique_ptr<asset_editor>>* editors{};
            std::unordered_map<uuid, uuid>* uniqueEditors{};
        };

        struct replace_unique_editor
        {
            bool update(const window_update_context& ctx) const
            {
                bool isOpen = true;
                bool save = false;
                bool cancel = false;

                constexpr auto popupName = "Save and close?##save_and_close";

                ImGui::OpenPopup(popupName);

                if (ImGui::BeginPopupModal(popupName))
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
                    if (save)
                    {
                        assetEditorManager->save_asset(ctx.windowManager, assetId).assert_value();
                    }

                    if (!cancel)
                    {
                        assetEditorManager->close_unique_type_editor(ctx.windowManager, descriptor.assetType);
                        assetEditorManager->open_editor(ctx.windowManager, assetId, descriptor.assetType)
                            .assert_value();
                    }
                }

                return isOpen;
            }

            asset_editor_manager* assetEditorManager{};
            asset_editor_descriptor descriptor{};
            uuid assetId{};
        };
    }

    asset_editor_manager::asset_editor_manager(window_handle root) : m_root{root}
    {
        auto& mm = module_manager::get();

        deque<asset_editor_descriptor> createDescs;

        for (auto* const createProvider : mm.find_services<asset_editor_provider>())
        {
            createDescs.clear();
            createProvider->fetch(createDescs);

            m_descriptors.append(createDescs.begin(), createDescs.end());
        }
    }

    asset_editor_manager::~asset_editor_manager() = default;

    expected<success_tag, asset_editor_manager::open_error> asset_editor_manager::open_editor(
        window_manager& wm, const uuid& assetId, const uuid& assetType)
    {
        const auto [it, inserted] = m_editors.emplace(assetId, unique_ptr<asset_editor>{});

        if (!inserted)
        {
            return open_error::already_open;
        }

        for (const auto& desc : m_descriptors)
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

                if (assetEditor && assetEditor->open(wm, m_root, assetId))
                {
                    const auto root = assetEditor->get_window();

                    const auto subscription = wm.create_child_window<destroy_subscription>(root);
                    auto* const ptr = wm.try_access<destroy_subscription>(subscription);
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

    void asset_editor_manager::close_editor(window_manager& wm, const uuid& assetId)
    {
        const auto it = m_editors.find(assetId);

        if (it == m_editors.end())
        {
            return;
        }

        it->second->close(wm);
    }

    void asset_editor_manager::close_unique_type_editor(window_manager& wm, const uuid& assetType)
    {
        const auto uIt = m_uniqueEditors.find(assetType);

        if (uIt == m_uniqueEditors.end())
        {
            return;
        }

        close_editor(wm, uIt->second);
    }

    expected<> asset_editor_manager::save_asset(window_manager& wm, const uuid& assetId)
    {
        const auto it = m_editors.find(assetId);

        if (it == m_editors.end())
        {
            return unspecified_error;
        }

        return it->second->save(wm);
    }
}