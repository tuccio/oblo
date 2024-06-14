#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/reflection/reflection_data.hpp>

#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>

#include <deque>
#include <functional>
#include <memory_resource>
#include <unordered_map>
#include <vector>

namespace oblo::reflection
{
    class any_attribute
    {
    public:
        any_attribute(
            std::pmr::unsynchronized_pool_resource* pool, void* ptr, void (*destroy)(void*), u32 size, u32 alignment) :
            m_pool{pool},
            m_ptr{ptr}, m_destroy{destroy}, m_size{size}, m_alignment{alignment}
        {
        }

        any_attribute(const any_attribute&) = delete;

        any_attribute(any_attribute&& other) noexcept
        {
            std::swap(m_pool, other.m_pool);
            std::swap(m_ptr, other.m_ptr);
            std::swap(m_destroy, other.m_destroy);
            std::swap(m_size, other.m_size);
            std::swap(m_alignment, other.m_alignment);
        }

        any_attribute& operator=(const any_attribute&) = delete;
        any_attribute& operator=(any_attribute&&) noexcept = delete;

        ~any_attribute()
        {
            m_destroy(m_ptr);
            m_pool->deallocate(m_ptr, m_size, m_alignment);
        }

        void* get() const
        {
            return m_ptr;
        }

    private:
        std::pmr::unsynchronized_pool_resource* m_pool{};
        void* m_ptr{};
        void (*m_destroy)(void*){};
        u32 m_size{};
        u32 m_alignment{};
    };

    struct fundamental_tag
    {
    };

    struct class_data
    {
        type_id type;
        std::vector<field_data> fields;
        std::deque<any_attribute> attributeStorage;
    };

    struct enum_data
    {
        type_id type;
        dynamic_array<std::string_view> names;
        dynamic_array<std::byte> values;
        type_id underlyingType;
    };

    struct reflection_registry_impl
    {
        std::pmr::unsynchronized_pool_resource pool;

        ecs::type_registry typesRegistry;
        ecs::entity_registry registry{&typesRegistry};

        std::unordered_map<type_id, ecs::entity> typesMap;
    };
}