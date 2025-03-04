#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/stack_allocator.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/utility/filter_components.hpp>

namespace oblo::ecs
{
    class deferred
    {
    public:
        deferred();
        deferred(allocator* a);
        deferred(const deferred&) = delete;
        deferred(deferred&&) noexcept = default;

        ~deferred()
        {
            OBLO_ASSERT(m_commands.empty());
        }

        deferred& operator=(const deferred&) = delete;
        deferred& operator=(deferred&&) noexcept = default;

        template <typename... ComponentsOrTags>
        decltype(auto) create();

        template <typename... ComponentsOrTags>
        decltype(auto) add(entity e);

        void add(entity e, const component_and_tag_sets& types);

        template <typename... ComponentsOrTags>
        void remove(entity e);

        void destroy(entity e);

        void apply(entity_registry& reg);
        void clear();

    private:
        template <typename T>
        T* allocate_storage();

        template <bool Create, typename... ComponentsOrTags>
        decltype(auto) add_or_create(entity e);

    private:
        struct command
        {
            using apply_fn = void (*)(entity_registry& registry, void* userdata);

            void* userdata{};
            apply_fn apply{};
        };

        using storage = stack_only_allocator<1u << 14, alignof(std::max_align_t), false>;

    private:
        deque<storage> m_storage;
        deque<command> m_commands;
    };

    inline deferred::deferred() : deferred{get_global_allocator()} {}

    inline deferred::deferred(allocator* a) : m_storage{a}, m_commands{a} {}

    template <typename... ComponentsOrTags>
    decltype(auto) deferred::create()
    {
        return add_or_create<true, ComponentsOrTags...>({});
    }

    template <typename... ComponentsOrTags>
    decltype(auto) deferred::add(entity e)
    {
        return add_or_create<false, ComponentsOrTags...>(e);
    }

    template <typename T>
    T* deferred::allocate_storage()
    {
        static_assert(sizeof(T) < storage::size);

        constexpr usize size = sizeof(T);
        constexpr usize alignment = alignof(T) < storage::alignment ? storage::alignment : alignof(T);

        if (m_storage.empty())
        {
            m_storage.emplace_back();
        }

        auto* ptr = m_storage.back().allocate(size, alignment);

        if (ptr == nullptr)
        {
            m_storage.emplace_back();
            ptr = m_storage.back().allocate(size, alignment);
        }

        OBLO_ASSERT(ptr);

        return new (ptr) T;
    }

    template <bool Create, typename... ComponentsOrTags>
    inline decltype(auto) deferred::add_or_create(entity e)
    {
        using tuple_t = filter_components<ComponentsOrTags...>::value_tuple;

        // This is used when the command is an add
        struct add_command_data
        {
            entity e;
            tuple_t components;

            void set_entity(entity entity)
            {
                e = entity;
            }

            entity get_or_create_entity(entity_registry&) const
            {
                return e;
            }
        };

        // This is used when the command is a create
        struct create_command_data
        {
            tuple_t components;

            void set_entity(entity) const {}

            entity get_or_create_entity(entity_registry& registry) const
            {
                return registry.create<ComponentsOrTags...>();
            }
        };

        using add_or_create_command_data = std::conditional_t<Create, create_command_data, add_command_data>;

        add_or_create_command_data* const data = allocate_storage<add_or_create_command_data>();
        data->set_entity(e);

        m_commands.push_back_default() = {
            .userdata = data,
            .apply =
                [](entity_registry& registry, void* userdata)
            {
                add_or_create_command_data* const data = static_cast<add_or_create_command_data*>(userdata);

                const entity e = data->get_or_create_entity(registry);

                if constexpr (std::tuple_size_v<tuple_t> == 0)
                {
                    // When no components are added (e.g. only tags) entity_registry::add returns void
                    // Nothing to do here
                    registry.add<ComponentsOrTags...>(e);
                }
                else if constexpr (std::tuple_size_v<tuple_t> == 1)
                {
                    // When we only have 1 component, entity_registry::add returns a reference to it
                    auto& newComponent = registry.add<ComponentsOrTags...>(e);

                    // Move component over
                    auto& srcComponent = std::get<0>(data->components);
                    newComponent = std::move(srcComponent);
                }
                else
                {
                    auto&& components = registry.add<ComponentsOrTags...>(e);

                    std::apply(
                        [&components]<typename... T>(T&... component)
                        {
                            // Move component over
                            ((std::get<T&>(components) = std::move(component)), ...);
                        },
                        data->components);
                }

                // Cleanup
                data->~add_or_create_command_data();
            },
        };

        if constexpr (std::tuple_size_v<tuple_t> == 0)
        {
            return;
        }
        else if constexpr (std::tuple_size_v<tuple_t> == 1)
        {
            return std::get<0>(data->components);
        }
        else
        {
            return std::apply([]<typename... T>(T&... component) { return std::tuple<T&...>{component...}; },
                data->components);
        }
    }

    template <typename... ComponentsOrTags>
    void deferred::remove(entity e)
    {
        struct remove_command_data
        {
            entity e;
        };

        remove_command_data* const data = allocate_storage<remove_command_data>();
        data->e = e;

        m_commands.push_back_default() = {
            .userdata = data,
            .apply =
                [](entity_registry& registry, void* userdata)
            {
                remove_command_data* const data = static_cast<remove_command_data*>(userdata);
                registry.remove<ComponentsOrTags...>(data->e);
                data->~remove_command_data();
            },
        };
    }

    inline void deferred::add(entity e, const component_and_tag_sets& types)
    {
        struct erased_add_data
        {
            entity e;
            component_and_tag_sets types;
        };

        erased_add_data* const data = allocate_storage<erased_add_data>();
        data->e = e;
        data->types = types;

        m_commands.push_back_default() = {
            .userdata = data,
            .apply =
                [](entity_registry& registry, void* userdata)
            {
                erased_add_data* const data = static_cast<erased_add_data*>(userdata);
                registry.add(data->e, data->types);
            },
        };
    }

    inline void deferred::destroy(entity e)
    {
        struct destroy_command_data
        {
            entity e;
        };

        destroy_command_data* const data = allocate_storage<destroy_command_data>();
        data->e = e;

        m_commands.push_back_default() = {
            .userdata = data,
            .apply =
                [](entity_registry& registry, void* userdata)
            {
                destroy_command_data* const data = static_cast<destroy_command_data*>(userdata);
                registry.destroy(data->e);
                data->~destroy_command_data();
            },
        };
    }

    inline void deferred::apply(entity_registry& reg)
    {
        for (auto& command : m_commands)
        {
            command.apply(reg, command.userdata);
        }

        m_commands.clear();
        m_storage.clear();
    }

    inline void deferred::clear()
    {
        m_commands.clear();
        m_storage.clear();
    }
}