#include <oblo/editor/window_manager.hpp>

#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    window_manager::window_manager() = default;

    window_manager::~window_manager() = default;

    void window_manager::update()
    {
        usize count = m_windows.size();

        for (usize i = 0; i < count;)
        {
            auto& window = m_windows[i];

            if (!window->update())
            {
                std::swap(window, m_windows.back());
                --count;
            }
            else
            {
                ++i;
            }
        }

        m_windows.resize(count);
    }
}