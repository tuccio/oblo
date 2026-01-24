#include <oblo/scene/editor/scene_editor.hpp>

#include <oblo/editor/window_manager.hpp>
#include <oblo/scene/editor/scene_editing_window.hpp>

namespace oblo::editor
{
    expected<> scene_editor::open(window_manager& wm, asset_registry& assetRegistry, window_handle parent, uuid assetId)
    {
        const auto h = wm.create_child_window<scene_editing_window>(parent, {}, service_registry{});

        if (!h)
        {
            return "Editor operation failed"_err;
        }

        auto* const sceneEditor = wm.try_access<scene_editing_window>(h);

        if (!sceneEditor || !sceneEditor->load_scene(assetRegistry, assetId))
        {
            wm.destroy_window(h);
            return "Editor operation failed"_err;
        }

        m_editor = h;
        m_assetId = assetId;

        return no_error;
    }

    void scene_editor::close(window_manager& wm)
    {
        wm.destroy_window(m_editor);
        m_editor = {};
    }

    expected<> scene_editor::save(window_manager& wm, asset_registry& assetRegistry)
    {
        auto* const sceneEditor = wm.try_access<scene_editing_window>(m_editor);

        if (!sceneEditor)
        {
            return "Editor operation failed"_err;
        }

        return sceneEditor->save_scene(assetRegistry);
    }

    window_handle scene_editor::get_window() const
    {
        return m_editor;
    }
}