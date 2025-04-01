#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/window_handle.hpp>

#include <span>
#include <unordered_map>

namespace oblo
{
    class asset_registry;
}

namespace oblo::editor
{
    class window_manager;
    class asset_editor;
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
        explicit asset_editor_manager(asset_registry& assetRegistry);
        ~asset_editor_manager();

        void set_window_root(window_handle root);

        expected<success_tag, open_error> open_editor(window_manager& wm, const uuid& assetId, const uuid& assetType);

        uuid find_unique_type_editor(const uuid& assetType);

        void close_editor(window_manager& wm, const uuid& assetId);

        expected<> save_asset(window_manager& wm, const uuid& assetId);

        window_handle get_window(const uuid& assetId) const;

    private:
        asset_registry& m_assetRegistry;
        std::unordered_map<uuid, unique_ptr<asset_editor>> m_editors;
        std::unordered_map<uuid, uuid> m_uniqueEditors;
        deque<asset_editor_descriptor> m_descriptors;
        window_handle m_root{};
    };
}