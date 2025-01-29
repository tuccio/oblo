#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/editor/window_handle.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <memory_resource>

namespace oblo
{
    class service_registry;
}

namespace oblo::editor
{
    class window_module;
    struct window_entry;
    struct window_update_context;

    enum class window_flags : u8
    {
        unique_sibling,
        enum_max,
    };

    class window_manager
    {
    public:
        window_manager();
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

        template <typename T, typename... Args>
        window_handle create_window(service_registry&& services, flags<window_flags> flags, Args&&... args);

        template <typename T>
        window_handle create_child_window(window_handle parent);

        template <typename T, typename... Args>
        window_handle create_child_window(window_handle parent, flags<window_flags> flags, Args&&... args);

        template <typename T>
        bool has_child(window_handle parent, bool recursive) const;

        template <typename T>
        window_handle find_child(window_handle parent, bool recursive) const;

        void destroy_window(window_handle handle);

        void update();

        template <typename T>
        T* try_access(window_handle handle) const;

        type_id get_window_type(window_handle handle) const;

        u8* get_window_pointer(window_handle handle) const;

        window_handle get_parent(window_handle handle) const;

    private:
        using memory_pool = std::pmr::unsynchronized_pool_resource;

        using update_fn = bool (*)(u8*, const window_update_context& ctx);
        using destroy_fn = void (*)(memory_pool& pool, u8*);

    private:
        template <typename T, typename... Args>
        window_handle create_window_impl(window_entry* parent, service_registry* overrideCtx, Args&&... args);

        window_handle create_window_impl(window_entry* parent,
            const type_id& type,
            service_registry* overrideCtx,
            u8* ptr,
            update_fn update,
            destroy_fn destroy,
            string_view debugName);

        window_handle find_child_impl(window_entry* parent, const type_id& type, bool recursive) const;

        window_entry* update_window(window_entry* entry);

        void connect(window_entry* parent, window_entry* child);
        void disconnect(window_entry* child);

        window_update_context make_window_update_context(window_handle handle);

        service_registry* create_new_registry(service_registry&& services);

    private:
        memory_pool m_pool;
        window_entry* m_root{};
        dynamic_array<unique_ptr<window_module>> m_windowModules;
    };

    template <typename T>
    window_handle window_manager::create_window(service_registry&& services)
    {
        return create_window_impl<T>(m_root, create_new_registry(std::move(services)));
    }

    template <typename T, typename... Args>
    window_handle window_manager::create_window(service_registry&& services, flags<window_flags> flags, Args&&... args)
    {
        if (flags.contains(window_flags::unique_sibling) && find_child_impl(m_root, get_type_id<T>(), false))
        {
            return {};
        }

        return create_window_impl<T>(m_root, create_new_registry(std::move(services)), std::forward<Args>(args)...);
    }

    template <typename T>
    window_handle window_manager::create_child_window(window_handle parent)
    {
        return create_child_window<T>(parent, flags<window_flags>{});
    }

    template <typename T, typename... Args>
    window_handle window_manager::create_child_window(window_handle parent, flags<window_flags> flags, Args&&... args)
    {
        if (flags.contains(window_flags::unique_sibling) && has_child<T>(parent, false))
        {
            return {};
        }

        auto* const parentEntry = reinterpret_cast<window_entry*>(parent.value);
        return create_window_impl<T>(parentEntry, nullptr, std::forward<Args>(args)...);
    }

    template <typename T>
    bool window_manager::has_child(window_handle parent, bool recursive) const
    {
        return bool{find_child<T>(parent, recursive)};
    }

    template <typename T>
    window_handle window_manager::find_child(window_handle parent, bool recursive) const
    {
        auto* const parentEntry = reinterpret_cast<window_entry*>(parent.value);
        return find_child_impl(parentEntry, get_type_id<T>(), recursive);
    }

    template <typename T, typename... Args>
    window_handle window_manager::create_window_impl(
        window_entry* parentEntry, service_registry* overrideCtx, Args&&... args)
    {
        T* const window = new (m_pool.allocate(sizeof(T), alignof(T))) T{std::forward<Args>(args)...};
        u8* const ptr = reinterpret_cast<u8*>(window);

        const update_fn update = [](u8* ptr, const window_update_context& ctx)
        { return reinterpret_cast<T*>(ptr)->update(ctx); };

        const auto newHandle = create_window_impl(
            parentEntry,
            get_type_id<T>(),
            overrideCtx,
            ptr,
            update,
            [](memory_pool& pool, u8* ptr)
            {
                reinterpret_cast<T*>(ptr)->~T();
                pool.deallocate(ptr, sizeof(T), alignof(T));
            },
            get_type_id<T>().name);

        if constexpr (requires(T& w, const window_update_context& ctx, bool b) { b = w.init(ctx); })
        {
            if (!window->init(make_window_update_context(newHandle)))
            {
                destroy_window(newHandle);
            }
        }
        else if constexpr (requires(T& w, const window_update_context& ctx) { w.init(ctx); })
        {
            window->init(make_window_update_context(newHandle));
        }

        return newHandle;
    }

    template <typename T>
    T* window_manager::try_access(window_handle handle) const
    {
        if (!handle)
        {
            return {};
        }

        const auto type = get_window_type(handle);

        if (type != get_type_id<T>())
        {
            return {};
        }

        return reinterpret_cast<T*>(get_window_pointer(handle));
    }
}