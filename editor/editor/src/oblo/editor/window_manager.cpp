#include <oblo/editor/window_manager.hpp>

namespace oblo::editor
{
    void window_manager::destroy_window(void* ptr)
    {
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it)
        {
            if (it->ptr.get() == ptr)
            {
                if (it != m_windows.end() - 1)
                {
                    std::swap(*it, m_windows.back());
                }

                m_windows.pop_back();
            }
        }
    }

    void window_manager::update()
    {
        for (usize i = 0; i < m_windows.size();)
        {
            auto& window = m_windows[i];

            if (!window.update(window.ptr.get()))
            {
                if (i != m_windows.size() - 1)
                {
                    std::swap(window, m_windows.back());
                }

                m_windows.pop_back();
            }
            else
            {
                ++i;
            }
        }
    }

    void window_manager::shutdown()
    {
        m_windows.clear();
    }
}