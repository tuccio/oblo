#pragma once

#include <oblo/core/string_interner.hpp>
#include <oblo/core/type_id.hpp>

#include <unordered_map>

namespace oblo::reflection
{
    class reflection_registry;
    struct field_data;
    struct type_handle;
}

namespace oblo
{
    enum class property_kind : u8;
    struct property_tree;

    class property_registry
    {
    public:
        property_registry();
        property_registry(const property_registry&) = delete;
        property_registry(property_registry&&) noexcept = delete;

        property_registry& operator=(const property_registry&) = delete;
        property_registry& operator=(property_registry&&) noexcept = delete;

        ~property_registry();

        void init(const reflection::reflection_registry& reflection);
        void shutdown();

        property_kind find_property_kind(const type_id& type) const;

        const property_tree* build_from_reflection(const type_id& type);

        const property_tree* try_get(const type_id& type) const;

    private:
        void build_recursive(property_tree& tree, u32 currentNodeIndex, reflection::type_handle type);

        bool try_add_property(property_tree& tree, u32 currentNodeIndex, const reflection::field_data& field);

    private:
        const reflection::reflection_registry* m_reflection{};
        std::unordered_map<type_id, property_tree> m_propertyTrees;
        std::unordered_map<type_id, property_kind> m_kindLookups;
    };
}