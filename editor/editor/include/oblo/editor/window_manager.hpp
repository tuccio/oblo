#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <memory>
#include <vector>

namespace oblo::editor
{
    class window_manager
    {
    public:
        window_manager() = default;
        window_manager(const window_manager&) = delete;
        window_manager(window_manager&&) noexcept = delete;

        ~window_manager() = default;

        window_manager& operator=(const window_manager&) = delete;
        window_manager& operator=(window_manager&&) noexcept = delete;

        template <typename T, typename... Args>
        T* create_window(Args&&... args);

        void destroy_window(void* ptr);

        void update();

        void shutdown();

    private:
        struct window_entry;

    private:
        std::vector<window_entry> m_windows;
    };

    struct window_manager::window_entry
    {
        using owning_ptr = std::unique_ptr<char, void (*)(char*)>;
        using update_fn = bool (*)(char*);

        owning_ptr ptr;
        update_fn update;
    };

    template <typename T, typename... Args>
    T* window_manager::create_window(Args&&... args)
    {
        T* ptr = new T{std::forward<Args>(args)...};

        m_windows.emplace_back(
            window_entry::owning_ptr{reinterpret_cast<char*>(ptr), [](char* ptr) { delete reinterpret_cast<T*>(ptr); }},
            [](char* ptr) { return reinterpret_cast<T*>(ptr)->update(); });

        return ptr;
    }
}