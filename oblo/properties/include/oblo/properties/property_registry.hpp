#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo::reflection
{
    class reflection_registry;
}

namespace oblo
{
    enum class property_kind : u8;
    struct property_tree;

    namespace meta_properties
    {
        constexpr cstring_view prefix = "$";
        constexpr cstring_view array_size = "$size";
        constexpr cstring_view array_element = "$element";
    }

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

        property_kind find_property_kind(const type_id& type) const;

        const property_tree* build_from_reflection(const type_id& type);
        bool try_build_from_reflection(property_tree& tree, const type_id& type) const;

        const property_tree* try_get(const type_id& type) const;

        const reflection::reflection_registry& get_reflection_registry() const;

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}