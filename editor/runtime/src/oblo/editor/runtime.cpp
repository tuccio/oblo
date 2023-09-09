#include <oblo/editor/runtime.hpp>

#include <oblo/editor/window.hpp>

namespace oblo::editor
{
    runtime::runtime() = default;

    runtime::~runtime() = default;

    void runtime::update()
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