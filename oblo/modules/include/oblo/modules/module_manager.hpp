#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>

#include <memory>
#include <unordered_map>

namespace oblo
{
    class module_interface;

    class MODULES_API module_manager
    {
    public:
        static module_manager& get();

    public:
        module_manager();
        module_manager(const module_manager&) = delete;
        module_manager(module_manager&&) noexcept = delete;

        module_manager& operator=(const module_manager&) = delete;
        module_manager& operator=(module_manager&&) noexcept = delete;

        ~module_manager();

        template <typename T>
        T* load();

        template <typename T>
        T* find() const;

        void shutdown();

    private:
        module_interface* find(const type_id& id) const;
        [[nodiscard]] bool load(const type_id& id, std::unique_ptr<module_interface> module);

    private:
        struct module_storage;

    private:
        std::unordered_map<type_id, module_storage> m_modules;
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
}