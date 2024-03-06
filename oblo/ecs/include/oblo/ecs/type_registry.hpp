#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/ecs/handles.hpp>

#include <span>
#include <unordered_map>

namespace oblo::ecs
{
    struct component_type_desc;
    struct tag_type_desc;

    class type_registry
    {
    public:
        type_registry();
        type_registry(const type_registry&) = delete;
        type_registry(type_registry&&) noexcept = default;
        type_registry& operator=(const type_registry&) = delete;
        type_registry& operator=(type_registry&&) noexcept = default;
        ~type_registry();

        component_type register_component(const component_type_desc& desc);
        component_type get_or_register_component(const component_type_desc& desc);

        tag_type get_or_register_tag(const tag_type_desc& desc);
        tag_type register_tag(const tag_type_desc& desc);

        component_type find_component(const type_id& type) const;
        tag_type find_tag(const type_id& type) const;

        template <typename T>
        component_type find_component() const;

        template <typename T>
        tag_type find_tag() const;

        const component_type_desc& get_component_type_desc(component_type type) const;
        const tag_type_desc& get_tag_type_desc(tag_type type) const;

        std::span<const component_type_desc> get_component_types() const;
        std::span<const tag_type_desc> get_tag_types() const;

    private:
        struct any_type_info;

    private:
        std::unordered_map<type_id, any_type_info> m_types;
        std::vector<component_type_desc> m_components;
        std::vector<tag_type_desc> m_tags;
    };

    template <typename T>
    component_type type_registry::find_component() const
    {
        return find_component(get_type_id<T>());
    }

    template <typename T>
    tag_type type_registry::find_tag() const
    {
        return find_tag(get_type_id<T>());
    }
}