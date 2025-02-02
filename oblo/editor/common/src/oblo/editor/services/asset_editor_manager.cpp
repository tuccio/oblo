#include <oblo/editor/services/asset_editor_manager.hpp>

#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/modules/module_manager.hpp>

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
                    editors->erase(id);
                }
            }

            bool update(const window_update_context&) const
            {
                return true;
            }

            uuid id{};
            std::unordered_map<uuid, window_handle>* editors{};
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
        const auto [it, inserted] = m_editors.emplace(assetId, window_handle{});

        if (!inserted)
        {
            return open_error::already_open;
        }

        for (const auto& desc : m_descriptors)
        {
            if (desc.assetType == assetType)
            {
                const auto h = desc.openEditorWindow(wm, assetId);

                if (h)
                {
                    it->second = h;

                    const auto subscription = wm.create_child_window<destroy_subscription>(h);
                    auto* const ptr = wm.try_access<destroy_subscription>(subscription);
                    OBLO_ASSERT(ptr);

                    if (ptr)
                    {
                        ptr->id = assetId;
                        ptr->editors = &m_editors;
                    }

                    return no_error;
                }
                else
                {
                    m_editors.erase(it);
                    return open_error::unspecified_error;
                }
            }
        }

        return open_error::no_such_type;
    }
}