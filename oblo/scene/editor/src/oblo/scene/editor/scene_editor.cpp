#include <oblo/scene/editor/scene_editor.hpp>

#include <oblo/editor/window_manager.hpp>
#include <oblo/scene/editor/scene_editing_window.hpp>

// if (!m_runtime.init({
//         .reflectionRegistry = &reflection->get_registry(),
//         .propertyRegistry = &propertyRegistry,
//         .resourceRegistry = &resourceRegistry,
//         .worldBuilders = mm.find_services<ecs::world_builder>(),
//     }))
//{
//     log::error("Failed to initialize runtime");
//     return false;
// }

namespace oblo::editor
{
    expected<> scene_editor::open(window_manager& wm, uuid)
    {
        const auto h = wm.create_window<scene_editing_window>({}, {});

        if (!h)
        {
            return unspecified_error;
        }

        m_editor = h;

        return no_error;
    }

    void scene_editor::close(window_manager& wm)
    {
        wm.destroy_window(m_editor);
        m_editor = {};
    }

    expected<> scene_editor::save(window_manager&)
    {
        return unspecified_error;
        // auto* const materialEditor = wm.try_access<scene_editing_window>(m_editor);

        // if (!materialEditor)
        //{
        //     return unspecified_error;
        // }

        // return materialEditor->save_asset();
    }

    window_handle scene_editor::get_window() const
    {
        return m_editor;
    }
}