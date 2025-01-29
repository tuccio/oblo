#pragma once

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

        deferred& operator=(const deferred&) = delete;
        deferred& operator=(deferred&&) noexcept = default;

        template <typename... ComponentsOrTags>
        decltype(auto) add(entity e);

        template <typename... ComponentsOrTags>
        void remove(entity e);

        void destroy(entity e);

        void apply(entity_registry& reg);

    private:
        template <typename T>
        T* allocate_storage();

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

    deferred::deferred() : deferred{get_global_allocator()} {}

    deferred::deferred(allocator* a) : m_storage{a}, m_commands{a} {}

    template <typename... ComponentsOrTags>
    decltype(auto) deferred::add(entity e)
    {
        using tuple_t = filter_components<ComponentsOrTags...>::tuple;
        tuple_t components;

        std::apply([this]<typename... T>(T*&... component) { ((component = allocate_storage<T>()), ...); }, components);

        struct add_command_data
        {
            entity e;
            tuple_t components;
        };

        add_command_data* const data = allocate_storage<add_command_data>();
        data->e = e;
        data->components = components;

        m_commands.push_back_default() = {
            .userdata = data,
            .apply =
                [](entity_registry& registry, void* userdata)
            {
                add_command_data* const data = static_cast<add_command_data*>(userdata);

                if constexpr (std::tuple_size_v<tuple_t> == 0)
                {
                    // When no components are added (e.g. only tags) entity_registry::add returns void
                    // Nothing to do here
                    registry.add<ComponentsOrTags...>(data->e);
                }
                else if constexpr (std::tuple_size_v<tuple_t> == 1)
                {
                    // When we only have 1 component, entity_registry::add returns a reference to it
                    auto& newComponent = registry.add<ComponentsOrTags...>(data->e);

                    // Move component over
                    auto* const srcComponent = std::get<0>(data->components);
                    newComponent = std::move(*srcComponent);

                    // Cleanup
                    std::destroy_at(srcComponent);
                }
                else
                {
                    auto&& components = registry.add<ComponentsOrTags...>(data->e);

                    std::apply(
                        [&components]<typename... T>(T*... component)
                        {
                            // Move component over
                            ((std::get<T&>(components) = std::move(*component)), ...);

                            // Cleanup
                            ((component->~T()), ...);
                        },
                        data->components);
                }

                data->~add_command_data();
            },
        };

        if constexpr (std::tuple_size_v<tuple_t> == 0)
        {
            return;
        }
        else if constexpr (std::tuple_size_v<tuple_t> == 1)
        {
            return *std::get<0>(components);
        }
        else
        {
            return std::apply([]<typename... T>(T*... component) { return std::tuple<T&...>{*component...}; },
                components);
        }
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

    void deferred::destroy(entity e)
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

    void deferred::apply(entity_registry& reg)
    {
        for (auto& command : m_commands)
        {
            command.apply(reg, command.userdata);
        }

        m_commands.clear();
        m_storage.clear();
    }
}