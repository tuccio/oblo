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
            }

            bool update(const window_update_context&) const
            {
                return true;
            }

            uuid id{};
            std::unordered_map<uuid, unique_ptr<asset_editor>>* editors{};
        };

        struct replace_unique_editor
        {
            bool update(const window_update_context&) const
            {
                bool isOpen = true;

                if (ImGui::BeginPopupModal("Save and close?"))
                {
                    ImGui::TextUnformatted("An asset is being closed, do you wish to save it before closing?");

                    if (ImGui::Button("Save"))
                    {
                        // TODO: Save and mark to open new editor
                        isOpen = false;
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Discard"))
                    {
                        // TODO: Mark to open new editor
                        isOpen = false;
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel"))
                    {
                        // TODO: Just close this window
                        isOpen = false;
                    }

                    ImGui::SameLine();

                    ImGui::EndPopup();
                }

                return isOpen;
            }

            asset_editor_descriptor descriptor{};
            uuid assetId{};
        };
    }

    asset_editor_manager::asset_editor_manager()
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

    const deque<asset_editor_descriptor>& asset_editor_manager::get_available_editors() const
    {
        return m_descriptors;
    }

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
                            ptr->descriptor = desc;
                            ptr->assetId = assetId;
                        }

                        return no_error;
                    }
                }

                unique_ptr assetEditor = desc.createEditor();

                if (assetEditor && assetEditor->open(wm, assetId))
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
}