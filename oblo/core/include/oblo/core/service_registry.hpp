#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/type_id.hpp>

#include <unordered_map>
#include <utility>

namespace oblo
{
    class service_registry_builder;

    template <typename>
    class service_builder;

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
        auto add();

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
        template <typename T, typename... Args>
        T* unique(Args&&... args)
        {
            T* const ptr = new T{std::forward<Args>(args)...};
            m_services.emplace_back(ptr, [](void* p) { delete static_cast<T*>(p); });
            return ptr;
        }

        template <typename B, typename T>
        void register_as(T* ptr)
        {
            m_map.emplace(get_type_id<B>(), const_cast<void*>(static_cast<const void*>(static_cast<B*>(ptr))));
        }

    private:
        friend class service_registry_builder;

        template <typename T>
        friend class service_builder;

    private:
        std::unordered_map<type_id, void*> m_map;
        deque<service> m_services;
    };

    template <typename T, typename... Bases>
    class [[nodiscard]] service_registry::builder
    {
    public:
        using service_type = T;

    public:
        template <typename... Args>
        auto unique(Args&&... args) &&
        {
            using U = std::remove_const_t<T>;

            auto* const ptr = m_registry->unique<U>(std::forward<Args>(args)...);
            (m_registry->register_as<Bases>(ptr), ...);
            return ptr;
        }

        T* unique(T&& s) &&
        {
            T* const ptr = m_registry->unique<T>(std::move(s));
            (m_registry->register_as<Bases>(ptr), ...);
            return ptr;
        }

        void externally_owned(T* ptr) &&
        {
            m_registry->m_services.emplace_back(const_cast<void*>(static_cast<const void*>(ptr)), nullptr);
            (m_registry->register_as<Bases>(ptr), ...);
        }

        template <typename B>
        builder<T, Bases..., B> as() const&&
        {
            return builder<T, Bases..., B>{m_registry};
        };

    private:
        friend class service_registry;
        friend class service_registry_builder;

        template <typename, typename...>
        friend class builder;

        explicit builder(service_registry* registry) : m_registry{registry} {}

    private:
        service_registry* m_registry;
    };

    template <typename T>
    auto service_registry::add()
    {
        using builder_t = std::conditional_t<std::is_const_v<T>,
            service_registry::builder<T, T>,
            service_registry::builder<T, T, std::add_const_t<T>>>;

        return builder_t{this};
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
