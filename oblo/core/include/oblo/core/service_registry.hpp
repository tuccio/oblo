#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/type_id.hpp>

#include <unordered_map>

namespace oblo
{
    struct service_entry
    {
        type_id type;
        void* pointer;
    };

    class service_registry
    {
    public:
        template <typename T, typename... Bases>
        class builder;

    public:
        service_registry() = default;
        service_registry(const service_registry&) = delete;
        service_registry(service_registry&&) noexcept = default;

        service_registry& operator=(const service_registry&) = delete;
        service_registry& operator=(service_registry&&) noexcept = default;

        ~service_registry();

        template <typename T>
        builder<T, T> add();

        template <typename T>
        T* find() const;

        void fetch_services(dynamic_array<service_entry>& out);

    private:
        struct service
        {
            void* ptr;
            void (*destroy)(void*);
        };

    private:
        std::unordered_map<type_id, void*> m_map;
        std::vector<service> m_services;
    };

    template <typename T, typename... Bases>
    class service_registry::builder
    {
    public:
        template <typename... Args>
        T* unique(Args&&... args) &&
        {
            T* const ptr = new T{std::forward<Args>(args)...};
            m_registry->m_services.emplace_back(ptr, [](void* p) { delete static_cast<T*>(p); });

            (m_registry->m_map.emplace(get_type_id<Bases>(), static_cast<Bases*>(ptr)), ...);
            return ptr;
        }

        void externally_owned(T* ptr) &&
        {
            m_registry->m_services.emplace_back(const_cast<void*>(static_cast<const void*>(ptr)), nullptr);

            (m_registry->m_map.emplace(get_type_id<Bases>(),
                 const_cast<void*>(static_cast<const void*>(static_cast<Bases*>(ptr)))),
                ...);
        }

        template <typename B>
        builder<T, Bases..., B> as() const&&
        {
            return builder<T, Bases..., B>{m_registry};
        };

    private:
        friend class service_registry;

        template <typename, typename...>
        friend class builder;

        explicit builder(service_registry* registry) : m_registry{registry} {}

    private:
        service_registry* m_registry;
    };

    inline service_registry::~service_registry()
    {
        for (auto it = m_services.rbegin(); it != m_services.rend(); ++it)
        {
            const auto [ptr, destroy] = *it;

            if (destroy && ptr)
            {
                destroy(ptr);
            }
        }
    }

    template <typename T>
    service_registry::builder<T, T> service_registry::add()
    {
        return builder<T, T>{this};
    }

    template <typename T>
    T* service_registry::find() const
    {
        const auto it = m_map.find(get_type_id<T>());
        return it == m_map.end() ? nullptr : static_cast<T*>(it->second);
    }

    inline void service_registry::fetch_services(dynamic_array<service_entry>& out)
    {
        out.reserve(out.size() + m_map.size());

        for (const auto& [type, pointer] : m_map)
        {
            out.emplace_back(type, pointer);
        }
    }
}
