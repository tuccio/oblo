#include <oblo/scene/resources/entity_hierarchy.hpp>

#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

namespace oblo
{
    entity_hierarchy::entity_hierarchy() = default;

    entity_hierarchy::~entity_hierarchy() = default;

    expected<> entity_hierarchy::init()
    {
        const auto reflectionRegistries = module_manager::get().find_services<const reflection::reflection_registry>();

        if (reflectionRegistries.empty() || reflectionRegistries.size() != 1)
        {
            return unspecified_error;
        }

        init(*reflectionRegistries.front());
        return no_error;
    }

    void entity_hierarchy::init(const reflection::reflection_registry& reflection)
    {
        ecs_utility::register_reflected_component_and_tag_types(reflection, &m_types, nullptr);
        m_registry.init(&m_types);
    }

    ecs::type_registry& entity_hierarchy::get_type_registry()
    {
        return m_types;
    }

    const ecs::type_registry& entity_hierarchy::get_type_registry() const
    {
        return m_types;
    }

    ecs::entity_registry& entity_hierarchy::get_entity_registry()
    {
        return m_registry;
    }

    const ecs::entity_registry& entity_hierarchy::get_entity_registry() const
    {
        return m_registry;
    }

    expected<> entity_hierarchy::load(cstring_view source)
    {
        const auto propertyRegistries = module_manager::get().find_services<const property_registry>();

        if (propertyRegistries.empty() || propertyRegistries.size() != 1)
        {
            return unspecified_error;
        }

        if (!init())
        {
            return unspecified_error;
        }

        data_document doc;

        if (!json::read(doc, source))
        {
            return unspecified_error;
        }

        if (!ecs_serializer::read(m_registry, doc, doc.get_root(), *propertyRegistries.front()))
        {
            return unspecified_error;
        }

        return no_error;
    }

    expected<> entity_hierarchy::save(cstring_view source) const
    {
        return save(source, {});
    }

    expected<> entity_hierarchy::save(cstring_view source, const ecs_serializer::write_config& cfg) const
    {
        const auto propertyRegistries = module_manager::get().find_services<const property_registry>();

        if (propertyRegistries.empty() || propertyRegistries.size() != 1)
        {
            return unspecified_error;
        }

        data_document doc;
        doc.init();

        if (!ecs_serializer::write(doc, doc.get_root(), m_registry, *propertyRegistries.front(), cfg))
        {
            return unspecified_error;
        }

        if (!json::write(doc, source))
        {
            return unspecified_error;
        }

        return no_error;
    }
}