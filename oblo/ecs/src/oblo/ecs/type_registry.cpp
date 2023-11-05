#include <oblo/ecs/type_registry.hpp>

#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/limits.hpp>
#include <oblo/ecs/tag_type_desc.hpp>

namespace oblo::ecs
{
    namespace
    {
        enum class type_kind : u8
        {
            component,
            tag,
        };
    }
    struct type_registry::any_type_info
    {
        u32 id;
        type_kind kind;
    };

    type_registry::type_registry() = default;

    type_registry::~type_registry() = default;

    component_type type_registry::register_component(const component_type_desc& desc)
    {
        if (m_components.size() >= MaxComponentTypes)
        {
            return {};
        }

        if (m_components.empty())
        {
            // Since we use 0 as invalid id, it's convenient to keep the indices the same as the id, so the first
            // element is just a dummy
            m_components.emplace_back();
        }

        component_type type{u32(m_components.size())};

        const auto [it, inserted] = m_types.emplace(desc.type,
            any_type_info{
                .id = type.value,
                .kind = type_kind::component,
            });

        if (!inserted)
        {
            return {};
        }

        m_components.emplace_back(desc);

        return type;
    }

    component_type type_registry::get_or_register_component(const component_type_desc& desc)
    {
        if (const auto c = find_component(desc.type))
        {
            return c;
        }

        return register_component(desc);
    }

    tag_type type_registry::register_tag(const tag_type_desc& desc)
    {
        if (m_tags.size() >= MaxComponentTypes)
        {
            return {};
        }

        if (m_tags.empty())
        {
            // Since we use 0 as invalid id, it's convenient to keep the indices the same as the id, so the first
            // element is just a dummy
            m_tags.emplace_back();
        }

        tag_type type{u32(m_tags.size())};

        const auto [it, inserted] = m_types.emplace(desc.type,
            any_type_info{
                .id = type.value,
                .kind = type_kind::tag,
            });

        if (!inserted)
        {
            return {};
        }

        m_tags.emplace_back(desc);

        return type;
    }

    tag_type type_registry::get_or_register_tag(const tag_type_desc& desc)
    {
        if (const auto t = find_tag(desc.type))
        {
            return t;
        }

        return register_tag(desc);
    }

    component_type type_registry::find_component(const type_id& type) const
    {
        const auto it = m_types.find(type);

        if (it == m_types.end() || it->second.kind != type_kind::component)
        {
            return {};
        }

        return {it->second.id};
    }

    tag_type type_registry::find_tag(const type_id& type) const
    {
        const auto it = m_types.find(type);

        if (it == m_types.end() || it->second.kind != type_kind::tag)
        {
            return {};
        }

        return {it->second.id};
    }

    const component_type_desc& type_registry::get_component_type_desc(component_type type) const
    {
        return m_components[type.value];
    }

    const tag_type_desc& type_registry::get_tag_type_desc(tag_type type) const
    {
        return m_tags[type.value];
    }
}