#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace oblo::editor
{
    class window;

    class window_manager
    {
    public:
        window_manager();
        window_manager(const window_manager&) = delete;
        window_manager(window_manager&&) noexcept = delete;

        ~window_manager();

        window_manager& operator=(const window_manager&) = delete;
        window_manager& operator=(window_manager&&) noexcept = delete;

        template <typename T, typename... Args>
        T* create_window(Args&&... args)
        {
            auto& w = m_windows.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            return static_cast<T*>(w.get());
        }

        void update();

    private:
        std::vector<std::unique_ptr<window>> m_windows;
    };
}