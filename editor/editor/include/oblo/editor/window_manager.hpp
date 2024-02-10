#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/editor/window_handle.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <memory_resource>
#include <vector>

namespace oblo
{
    class service_registry;
}

namespace oblo::editor
{
    struct window_entry;
    struct window_update_context;

    class window_manager
    {
    public:
        window_manager() = default;
        window_manager(const window_manager&) = delete;
        window_manager(window_manager&&) noexcept = delete;

        ~window_manager();

        window_manager& operator=(const window_manager&) = delete;
        window_manager& operator=(window_manager&&) noexcept = delete;

        void init();
        void shutdown();

        service_registry& get_global_service_registry();

        template <typename T>
        window_handle create_window(service_registry&& services);

        template <typename T>
        window_handle create_child_window(window_handle parent);

        void destroy_window(window_handle handle);

        void update();

    private:
        using memory_pool = std::pmr::unsynchronized_pool_resource;

        using update_fn = bool (*)(u8*, const window_update_context& ctx);
        using destroy_fn = void (*)(memory_pool& pool, u8*);

    private:
        template <typename T>
        window_handle create_window_impl(window_entry* parent, service_registry* overrideCtx);

        window_handle create_window_impl(
            window_entry* parent, service_registry* overrideCtx, u8* ptr, update_fn update, destroy_fn destroy);

        window_entry* update_window(window_entry* entry);

        void connect(window_entry* parent, window_entry* child);
        void disconnect(window_entry* child);

        window_update_context make_window_update_context(window_handle handle);

        service_registry* create_new_registry(service_registry&& services);

    private:
        memory_pool m_pool;
        window_entry* m_root{};
    };

    template <typename T>
    window_handle window_manager::create_window(service_registry&& services)
    {
        return create_window_impl<T>(m_root, create_new_registry(std::move(services)));
    }

    template <typename T>
    window_handle window_manager::create_child_window(window_handle parent)
    {
        auto* const parentEntry = reinterpret_cast<window_entry*>(parent.value);
        return create_window_impl<T>(parentEntry, nullptr);
    }

    template <typename T>
    window_handle window_manager::create_window_impl(window_entry* parentEntry, service_registry* overrideCtx)
    {
        T* const window = new (m_pool.allocate(sizeof(T), alignof(T))) T{};
        u8* const ptr = reinterpret_cast<u8*>(window);

        const update_fn update = [](u8* ptr, const window_update_context& ctx)
        { return reinterpret_cast<T*>(ptr)->update(ctx); };

        const auto newHandle = create_window_impl(parentEntry,
            overrideCtx,
            ptr,
            update,
            [](memory_pool& pool, u8* ptr)
            {
                reinterpret_cast<T*>(ptr)->~T();
                pool.deallocate(ptr, sizeof(T), alignof(T));
            });

        if constexpr (requires(T& w, const window_update_context& ctx) { w.init(ctx); })
        {
            window->init(make_window_update_context(newHandle));
        }

        return newHandle;
    }
}