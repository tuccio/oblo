#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/service_registry.hpp>

#include <functional>
#include <span>
#include <tuple>

namespace oblo
{
    template <typename T>
    struct service_requires
    {
    };

    enum class service_build_error
    {
        missing_dependency,
        circular_dependency,
        conflict,
    };

    class service_registry_builder
    {
    public:
        template <typename... B>
        struct bases
        {
        };

        template <typename... R>
        struct require
        {
        };

        template <typename T, typename...>
        class builder;

        template <typename T, typename... Bases, typename... Requires>
        class [[nodiscard]] builder<T, bases<Bases...>, require<Requires...>>
        {
        public:
            template <typename... B>
            auto as() const&&
            {
                return builder<T, bases<Bases..., B...>, service_registry_builder::require<Requires...>>{m_builder};
            };

            template <typename... R>
            auto require() const
            {
                return builder<T, bases<Bases...>, service_registry_builder ::require<Requires..., R...>>{m_builder};
            };

            template <typename F>
            void build(F&& f) &&
            {
                m_builder->register_builder(std::forward<F>(f), this);
            }

        private:
            friend class service_registry_builder;

            builder(service_registry_builder* b) : m_builder{b} {}

        private:
            service_registry_builder* m_builder{};
        };

    public:
        template <typename T>
        builder<T, bases<T, std::add_const_t<T>>, require<>> add()
        {
            return {this};
        }

        expected<success_tag, service_build_error> build(service_registry& serviceRegistry);

    private:
        struct builder_info
        {
            using get_type_fn = std::span<const type_id> (*)();

            std::function<void(service_registry&)> build;
            get_type_fn getRequires;
            get_type_fn getBases;
        };

        template <typename F, typename T, typename... Requires, typename... Bases>
        void register_builder(F&& f, const builder<T, bases<Bases...>, require<Requires...>>*);

    private:
        deque<builder_info> m_builders;
    };

    namespace detail
    {
        template <typename... T>
        std::span<const type_id> make_type_span()
        {
            if constexpr (sizeof...(T) > 0)
            {
                static constexpr type_id array[sizeof...(T)] = {get_type_id<T>()...};
                return array;
            }
            else
            {
                return {};
            }
        }

    }

    template <typename T>
    class service_builder
    {
        using register_fn = void (*)(T* ptr, service_registry&);

    public:
        template <typename... Bases>
        service_builder(service_registry& registry, service_registry_builder::bases<Bases...>) : m_registry{registry}
        {
            m_registerBases = +[](T* ptr, service_registry& reg) { (reg.register_as<Bases>(ptr), ...); };
        }

        template <typename... Args>
        void unique(Args&&... args)
        {
            T* const ptr = m_registry.unique<T>(std::forward<Args>(args)...);
            m_registerBases(ptr, m_registry);
        }

        template <typename S>
        S* find() const
        {
            return m_registry.find<S>();
        }

    private:
        service_registry& m_registry;
        register_fn m_registerBases{};
    };

    template <typename F, typename T, typename... Requires, typename... Bases>
    void service_registry_builder::register_builder(F&& f, const builder<T, bases<Bases...>, require<Requires...>>*)
    {
        m_builders.emplace_back([cb = std::forward<F>(f)](service_registry& registry)
            { cb(service_builder<T>{registry, bases<Bases...>{}}); },
            &detail::make_type_span<Requires...>,
            &detail::make_type_span<Bases...>);
    }
}