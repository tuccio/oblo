#include <oblo/reflection/reflection_registry.hpp>

#include <oblo/reflection/reflection_registry_impl.hpp>
#include <oblo/reflection/registration/registrant.hpp>

namespace oblo::reflection
{
    reflection_registry::reflection_registry() : m_impl{std::make_unique<reflection_registry_impl>()} {}

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
            const auto& type = m_impl->types[it->second.value];

            if (type.kind == type_kind::class_kind)
            {
                classId = class_handle{type.concreteIndex};
            }
        }

        return classId;
    }

    std::span<const field_data> reflection_registry::get_fields(class_handle classId) const
    {
        return m_impl->classes[classId.value].fields;
    }
}
