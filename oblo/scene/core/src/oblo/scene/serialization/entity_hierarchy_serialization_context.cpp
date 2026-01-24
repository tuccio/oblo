#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>

#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/reflection_registry.hpp>
#include <oblo/reflection/tags/serialization.hpp>
#include <oblo/scene/components/tags.hpp>

namespace oblo
{
    namespace
    {
        template <typename T>
        T* find_unique_service()
        {
            const auto res = module_manager::get().find_services<T>();

            if (res.empty() || res.size() != 1)
            {
                return nullptr;
            }

            return res.front();
        }
    }

    expected<> entity_hierarchy_serialization_context::init()
    {
        m_types = find_unique_service<const ecs::type_registry>();
        m_properties = find_unique_service<const property_registry>();

        if (!m_types || !m_properties)
        {
            return "Serialization failed"_err;
        }

        const auto& reflection = m_properties->get_reflection_registry();

        for (const auto& desc : m_types->get_component_types())
        {
            const auto reflectionTypeId = reflection.find_type(desc.type);

            if (reflectionTypeId && reflection.has_tag<reflection::transient_type_tag>(reflectionTypeId))
            {
                m_transientTypes.components.add(m_types->find_component(desc.type));
            }
        }

        for (const auto& desc : m_types->get_tag_types())
        {
            const auto reflectionTypeId = reflection.find_type(desc.type);

            if (reflectionTypeId && reflection.has_tag<reflection::transient_type_tag>(reflectionTypeId))
            {
                m_transientTypes.tags.add(m_types->find_tag(desc.type));
            }
        }

        return no_error;
    }

    const property_registry& entity_hierarchy_serialization_context::get_property_registry() const
    {
        OBLO_ASSERT(m_properties);
        return *m_properties;
    }

    const ecs::type_registry& entity_hierarchy_serialization_context::get_type_registry() const
    {
        OBLO_ASSERT(m_types);
        return *m_types;
    }

    ecs_serializer::write_config entity_hierarchy_serialization_context::make_write_config() const
    {
        ecs_serializer::write_config cfg{};

        cfg.skipEntities.tags.add(m_types->find_tag<transient_tag>());
        cfg.skipTypes = m_transientTypes;

        return cfg;
    }

    ecs_serializer::read_config entity_hierarchy_serialization_context::make_read_config() const
    {
        return {};
    }
}