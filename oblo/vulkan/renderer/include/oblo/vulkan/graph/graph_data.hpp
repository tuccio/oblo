#pragma once

#include <oblo/core/type_id.hpp>

#include <memory>
#include <string>

namespace oblo::vk
{
    class any
    {
    public:
        any() = default;
        any(const any&) = delete;
        any(any&& other) noexcept = default;

        template <typename T>
        explicit any(T&& value)
        {
            m_ptr.reset(new wrapper<std::decay_t<T>>{std::forward<T>(value)});
        }

        ~any() = default;

        any& operator=(const any&) = delete;
        any& operator=(any&& other) noexcept = default;

        void reset()
        {
            m_ptr.reset();
        }

        template <typename T, typename... Args>
        T& emplace(Args&&... args)
        {
            auto* ptr = new wrapper<T>{std::forward<Args>(args)...};
            m_ptr.reset(ptr);
            return ptr->value;
        }

        void* get()
        {
            return m_ptr->get();
        }

    private:
        struct any_interface
        {
            virtual ~any_interface() = default;
            virtual void* get() = 0;
        };

        template <typename T>
        struct wrapper final : any_interface
        {
            template <typename... Args>
            wrapper(Args&&... args) : value{std::forward<Args>(args)...}
            {
            }

            virtual void* get()
            {
                return &value;
            }

            T value;
        };

    private:
        std::unique_ptr<any_interface> m_ptr;
    };

    class runtime_builder;
    class runtime_context;

    using construct_node_fn = void (*)(void*);
    using destruct_node_fn = void (*)(void*);
    using build_fn = void (*)(void*, runtime_builder&);
    using execute_fn = void (*)(void*, runtime_context&);

    struct node
    {
        void* node;
        type_id typeId;
        construct_node_fn construct;
        destruct_node_fn destruct;
        build_fn build;
        execute_fn execute;
    };

    struct pin
    {
        any value;
        type_id typeId;
        std::string name;
    };

    struct gpu_resource
    {
        u32 id;
        type_id type;
    };

    struct cpu_data
    {
        void* ptr;
        type_id type;
    };
}