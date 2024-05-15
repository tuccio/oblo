#pragma once

#include <oblo/core/lifetime.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <memory>
#include <span>
#include <unordered_map>

namespace oblo
{
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

        template <typename T>
        T* find() const;

        MODULES_API void shutdown();

        template <typename T>
        std::span<T* const> find_services() const;

    private:
        MODULES_API module_interface* find(const type_id& id) const;
        [[nodiscard]] MODULES_API bool load(const type_id& id, std::unique_ptr<module_interface> module);

        MODULES_API std::span<void* const> find_services(const type_id& type) const;

    private:
        struct module_storage;
        struct service_storage;

    private:
        std::unordered_map<type_id, module_storage> m_modules;
        std::unordered_map<type_id, service_storage> m_services;
        u32 m_nextLoadIndex{};
    };

    template <typename T>
    T* module_manager::load()
    {
        constexpr auto id = get_type_id<T>();

        if (auto* const m = find(id))
        {
            return static_cast<T*>(m);
        }

        auto m = std::make_unique<T>();
        T* const ptr = m.get();

        if (!load(id, std::move(m)))
        {
            return nullptr;
        }

        return ptr;
    }

    template <typename T>
    T* module_manager::find() const
    {
        constexpr auto id = get_type_id<T>();

        if (auto* const m = find(id))
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