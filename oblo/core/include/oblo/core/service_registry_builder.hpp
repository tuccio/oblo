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
        class builder<T, bases<Bases...>, require<Requires...>>
        {
        public:
            template <typename B>
            auto as() const&&
            {
                return builder<T, bases<Bases..., B>, service_registry_builder::require<Requires...>>{m_builder};
            };

            template <typename R>
            auto require() const
            {
                return builder<T, bases<Bases...>, service_registry_builder ::require<Requires..., R>>{m_builder};
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

        template <typename T, typename, typename>
        struct concrete_builder;

        template <typename T, typename... Bases, typename... Requires>
        class concrete_builder<T, bases<Bases...>, require<Requires...>> : service_registry::builder<T, Bases...>
        {
            using base_builder = service_registry::builder<T, Bases...>;

        public:
            using base_builder::builder;

            template <typename R>
                requires((std::is_same_v<R, Requires> || ...))
            R& get() const
            {
                return *static_cast<const base_builder&>(*this).m_registry->find<R>();
            }

            template <typename... Args>
            void unique(Args&&... args)
            {
                static_cast<base_builder&&>(*this).unique(std::forward<Args...>(args)...);
            }
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
        void register_builder(F&& f, const builder<T, bases<Bases...>, require<Requires...>>*)
        {
            auto getRequires = []() -> std::span<const type_id>
            {
                if constexpr (sizeof...(Requires) > 0)
                {
                    static constexpr type_id array[sizeof...(Requires)] = {get_type_id<Requires>()...};
                    return array;
                }
                else
                {
                    return {};
                }
            };

            auto getBases = []() -> std::span<const type_id>
            {
                if constexpr (sizeof...(Bases) > 0)
                {
                    static constexpr type_id array[sizeof...(Bases)] = {get_type_id<Bases>()...};
                    return array;
                }
                else
                {
                    return {};
                }
            };

            m_builders.emplace_back([cb = std::forward<F>(f)](service_registry& registry)
                { cb(concrete_builder<T, bases<Bases...>, require<Requires...>>{&registry}); },
                getRequires,
                getBases);
        }

    private:
        deque<builder_info> m_builders;
    };
}