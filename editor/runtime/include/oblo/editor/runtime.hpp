#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace oblo::editor
{
    class window;

    class runtime
    {
    public:
        runtime();
        runtime(const runtime&) = delete;
        runtime(runtime&&) noexcept = delete;

        ~runtime();

        runtime& operator=(const runtime&) = delete;
        runtime& operator=(runtime&&) noexcept = delete;

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