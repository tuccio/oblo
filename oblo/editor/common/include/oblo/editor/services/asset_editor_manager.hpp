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
    class resource_registry;
}

namespace oblo::editor
{
    class window_manager;
    class asset_editor;
    class resource_viewer;
    struct asset_editor_descriptor;
    struct resource_viewer_descriptor;

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
        explicit asset_editor_manager(asset_registry& assetRegistry, const resource_registry& resourceRegistry);
        ~asset_editor_manager();

        void set_window_root(window_handle root);

        expected<success_tag, open_error> open_asset(window_manager& wm, const uuid& assetId, const uuid& assetType);
        expected<success_tag, open_error> open_resource(
            window_manager& wm, const uuid& resourceId, const uuid& resourceType);

        uuid find_unique_asset_editor(const uuid& assetType);

        void close_asset(window_manager& wm, const uuid& assetId);
        void close_resource(window_manager& wm, const uuid& resourceId);

        expected<> save_asset(window_manager& wm, const uuid& assetId);

        window_handle get_asset_window(const uuid& assetId) const;
        window_handle get_resource_window(const uuid& resourceId) const;

    private:
        asset_registry& m_assetRegistry;
        const resource_registry& m_resourceRegistry;
        std::unordered_map<uuid, unique_ptr<asset_editor>> m_editors;
        std::unordered_map<uuid, unique_ptr<resource_viewer>> m_viewers;
        std::unordered_map<uuid, uuid> m_uniqueEditors;
        deque<asset_editor_descriptor> m_editorDescriptors;
        deque<resource_viewer_descriptor> m_viewerDescriptors;
        window_handle m_root{};
    };
}