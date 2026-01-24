#include <oblo/scene/resources/entity_hierarchy.hpp>

#include <oblo/properties/serialization/common.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

namespace oblo
{

    entity_hierarchy::entity_hierarchy() = default;

    entity_hierarchy::entity_hierarchy(entity_hierarchy&&) noexcept = default;

    entity_hierarchy::~entity_hierarchy() = default;

    entity_hierarchy& entity_hierarchy::operator=(entity_hierarchy&&) noexcept = default;

    expected<> entity_hierarchy::init(const ecs::type_registry& typeRegistry)
    {
        m_registry.init(&typeRegistry);
        return no_error;
    }

    ecs::entity_registry& entity_hierarchy::get_entity_registry()
    {
        return m_registry;
    }

    const ecs::entity_registry& entity_hierarchy::get_entity_registry() const
    {
        return m_registry;
    }

    SCENE_API expected<> entity_hierarchy::load(cstring_view source, const entity_hierarchy_serialization_context& ctx)
    {
        return load(source, ctx.get_property_registry(), ctx.make_read_config());
    }

    expected<> entity_hierarchy::load(
        cstring_view source, const property_registry& propertyRegistry, const ecs_serializer::read_config& cfg)
    {
        data_document doc;

        if (!json::read(doc, source))
        {
            return "Entity not found"_err;
        }

        if (!ecs_serializer::read(m_registry, doc, doc.get_root(), propertyRegistry, {}, cfg))
        {
            return "Entity not found"_err;
        }

        return no_error;
    }

    expected<> entity_hierarchy::load(
        const data_document& doc, const property_registry& propertyRegistry, const ecs_serializer::read_config& cfg)
    {
        if (!ecs_serializer::read(m_registry, doc, doc.get_root(), propertyRegistry, {}, cfg))
        {
            return "Entity not found"_err;
        }

        return no_error;
    }

    expected<> entity_hierarchy::save(cstring_view source, const entity_hierarchy_serialization_context& ctx) const
    {
        return save(source, ctx.get_property_registry(), ctx.make_write_config());
    }

    expected<> entity_hierarchy::save(cstring_view destination,
        const property_registry& propertyRegistry,
        const ecs_serializer::write_config& cfg) const
    {
        data_document doc;
        doc.init();

        if (!ecs_serializer::write(doc, doc.get_root(), m_registry, propertyRegistry, cfg))
        {
            return "Entity not found"_err;
        }

        if (!json::write(doc, destination))
        {
            return "Failed to save scene"_err;
        }

        return no_error;
    }

    expected<> entity_hierarchy::save(
        data_document& doc, const property_registry& propertyRegistry, const ecs_serializer::write_config& cfg) const
    {
        if (!ecs_serializer::write(doc, doc.get_root(), m_registry, propertyRegistry, cfg))
        {
            return "Entity not found"_err;
        }

        return no_error;
    }

    expected<> entity_hierarchy::copy_from(const ecs::entity_registry& other,
        const property_registry& propertyRegistry,
        const ecs_serializer::write_config& wCfg,
        const ecs_serializer::read_config& rCfg)
    {
        if (&other == &m_registry)
        {
            return "Failed to load scene"_err;
        }

        data_document doc;
        doc.init();

        if (!ecs_serializer::write(doc, doc.get_root(), other, propertyRegistry, wCfg))
        {
            return "Entity not found"_err;
        }

        return load(doc, propertyRegistry, rCfg);
    }

    expected<> entity_hierarchy::copy_to(ecs::entity_registry& other,
        const property_registry& propertyRegistry,
        const ecs_serializer::write_config& wCfg,
        const ecs_serializer::read_config& rCfg) const
    {
        if (&other == &m_registry)
        {
            return "Failed to load scene"_err;
        }

        data_document doc;
        doc.init();

        if (!ecs_serializer::write(doc, doc.get_root(), m_registry, propertyRegistry, wCfg))
        {
            return "Entity not found"_err;
        }

        if (!ecs_serializer::read(other, doc, doc.get_root(), propertyRegistry, {}, rCfg))
        {
            return "Entity not found"_err;
        }

        return no_error;
    }
}