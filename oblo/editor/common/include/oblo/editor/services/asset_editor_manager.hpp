#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/window_handle.hpp>

#include <span>
#include <unordered_map>

namespace oblo::editor
{
    class window_manager;
    struct asset_editor_descriptor;

    class asset_editor_manager
    {
    public:
        enum class open_error : u8
        {
            opened,
            already_open,
            no_such_type,
            unspecified_error,
        };

    public:
        asset_editor_manager();
        ~asset_editor_manager();

        const deque<asset_editor_descriptor>& get_available_editors() const;

        expected<success_tag, open_error> open_editor(window_manager& wm, const uuid& assetId, const uuid& assetType);

    private:
        std::unordered_map<uuid, window_handle> m_editors;
        deque<asset_editor_descriptor> m_descriptors;
    };
}