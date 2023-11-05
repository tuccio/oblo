#include <oblo/reflection/reflection_registry.hpp>

#include <oblo/ecs/range.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/reflection/reflection_registry_impl.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    reflection_registry::reflection_registry() : m_impl{std::make_unique<reflection_registry_impl>()}
    {
        m_impl->typesRegistry.register_component(ecs::make_component_type_desc<type_data>());
        m_impl->typesRegistry.register_component(ecs::make_component_type_desc<class_data>());
        m_impl->typesRegistry.register_component(ecs::make_component_type_desc<fundamental_tag>());
    }

    reflection_registry::reflection_registry(reflection_registry&&) noexcept = default;

    reflection_registry& reflection_registry::operator=(reflection_registry&&) noexcept = default;

    reflection_registry::~reflection_registry() = default;

    reflection_registry::registrant reflection_registry::get_registrant()
    {
        return registrant{*this};
    }

    type_handle reflection_registry::find_type(const type_id& type) const
    {
        type_handle typeId{};

        const auto it = m_impl->typesMap.find(type);

        if (it != m_impl->typesMap.end())
        {
            const auto e = it->second;
            typeId = type_handle{e.value};
        }

        return typeId;
    }

    class_handle reflection_registry::find_class(const type_id& type) const
    {
        class_handle classId{};

        const auto it = m_impl->typesMap.find(type);

        if (it != m_impl->typesMap.end())
        {
            const auto e = it->second;

            if (m_impl->registry.has<class_data>(e))
            {
                classId = class_handle{e.value};
            }
        }

        return classId;
    }

    type_data reflection_registry::get_type_data(type_handle typeId) const
    {
        const ecs::entity e{typeId.value};
        return m_impl->registry.get<type_data>(e);
    }

    class_handle reflection_registry::try_get_class(type_handle typeId) const
    {
        const ecs::entity e{typeId.value};
        return e && m_impl->registry.try_get<class_data>(e) != nullptr ? class_handle{e.value} : class_handle{};
    }

    std::span<const field_data> reflection_registry::get_fields(class_handle classId) const
    {
        const auto e = ecs::entity{classId.value};
        return m_impl->registry.get<class_data>(e).fields;
    }

    void reflection_registry::find_by_tag(const type_id& tag, std::vector<type_handle>& types) const
    {
        const auto tagType = m_impl->typesRegistry.find_tag(tag);

        if (!tagType)
        {
            return;
        }

        ecs::component_and_tags_sets sets{};
        sets.tags.add(tagType);

        // TODO: (#10) A component is necessary here
        for (const auto [entities, _] : m_impl->registry.range<type_data>().with(sets))
        {
            for (const auto e : entities)
            {
                types.emplace_back(e.value);
            }
        }
    }

    const void* reflection_registry::find_concept(type_handle typeId, const type_id& type) const
    {
        const auto e = ecs::entity{typeId.value};
        const auto c = m_impl->typesRegistry.find_component(type);

        return c ? m_impl->registry.try_get(e, c) : nullptr;
    }

    bool reflection_registry::is_fundamental(type_handle typeId) const
    {
        return m_impl->registry.has<fundamental_tag>(ecs::entity{typeId.value});
    }
}
