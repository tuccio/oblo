#include <oblo/reflection/reflection_registry.hpp>

#include <oblo/ecs/utility/registration.hpp>
#include <oblo/reflection/reflection_registry_impl.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    reflection_registry::reflection_registry() : m_impl{std::make_unique<reflection_registry_impl>()}
    {
        m_impl->typesRegistry.register_component(ecs::make_component_type_desc<type_id>());
        m_impl->typesRegistry.register_component(ecs::make_component_type_desc<type_kind>());
        m_impl->typesRegistry.register_component(ecs::make_component_type_desc<class_data>());
    }

    reflection_registry::reflection_registry(reflection_registry&&) noexcept = default;

    reflection_registry& reflection_registry::operator=(reflection_registry&&) noexcept = default;

    reflection_registry::~reflection_registry() = default;

    reflection_registry::registrant reflection_registry::get_registrant()
    {
        return registrant{*this};
    }

    class_handle reflection_registry::find_class(const type_id& type) const
    {
        class_handle classId{};

        const auto it = m_impl->typesMap.find(type);

        if (it != m_impl->typesMap.end())
        {
            const auto e = it->second;

            if (m_impl->registry.try_get<class_data>(e))
            {
                classId = class_handle{e.value};
            }
        }

        return classId;
    }

    std::span<const field_data> reflection_registry::get_fields(class_handle classId) const
    {
        const auto e = ecs::entity{classId.value};
        return m_impl->registry.get<class_data>(e).fields;
    }
}
