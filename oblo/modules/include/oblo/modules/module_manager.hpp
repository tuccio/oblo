#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <span>
#include <unordered_map>

namespace oblo
{
    class string;
    class module_interface;

    class module_manager
    {
    public:
        MODULES_API static module_manager& get();

    public:
        MODULES_API module_manager();
        module_manager(const module_manager&) = delete;
        module_manager(module_manager&&) noexcept = delete;

        module_manager& operator=(const module_manager&) = delete;
        module_manager& operator=(module_manager&&) noexcept = delete;

        MODULES_API ~module_manager();

        template <typename T>
        T* load();

        MODULES_API module_interface* load(cstring_view path);

        template <typename T>
        T* find() const;

        MODULES_API void finalize();

        MODULES_API void shutdown();

        template <typename T>
        std::span<T* const> find_services() const;

    private:
        struct module_storage;
        struct scoped_state_change;
        struct service_storage;

        enum class state : u8
        {
            idle,
            loading,
            finalizing,
            finalized,
        };

    private:
        MODULES_API module_interface* find(const hashed_string_view& id) const;
        [[nodiscard]] MODULES_API module_storage* load(const hashed_string_view& id,
            unique_ptr<module_interface> module);

        MODULES_API std::span<void* const> find_services(const type_id& type) const;

    private:
        std::unordered_map<hashed_string_view, module_storage, hash<hashed_string_view>> m_modules;
        std::unordered_map<type_id, service_storage> m_services;
        deque<string> m_loadPaths;
        u32 m_nextLoadIndex{};
        state m_state{state::idle};
    };

    template <typename T>
    T* module_manager::load()
    {
        OBLO_ASSERT(m_state <= state::loading);

        constexpr auto id = get_type_id<T>();

        if (auto* const m = find(id.name))
        {
            return static_cast<T*>(m);
        }

        auto m = allocate_unique<T>();
        T* const ptr = m.get();

        if (!load(id.name, std::move(m)))
        {
            return nullptr;
        }

        return ptr;
    }

    template <typename T>
    T* module_manager::find() const
    {
        constexpr auto id = get_type_id<T>();

        if (auto* const m = find(id.name))
        {
            return static_cast<T*>(m);
        }

        return nullptr;
    }

    template <typename T>
    inline std::span<T* const> module_manager::find_services() const
    {
        const auto services = find_services(get_type_id<T>());

        if (services.empty())
        {
            return {};
        }

        const auto count = services.size();

        auto* const ptr = start_lifetime_as_array<T*>(services.data(), count);
        return {ptr, count};
    }
}