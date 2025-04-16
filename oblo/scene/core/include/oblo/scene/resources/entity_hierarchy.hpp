#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/reflection/codegen/annotations.hpp>

namespace oblo
{
    class data_document;
    class entity_hierarchy_serialization_context;
    class property_registry;

    namespace reflection
    {
        class reflection_registry;
    }

    namespace ecs_serializer
    {
        struct read_config;
        struct write_config;
    }

    class entity_hierarchy
    {
    public:
        SCENE_API entity_hierarchy();
        entity_hierarchy(const entity_hierarchy&) = delete;
        entity_hierarchy(entity_hierarchy&&) noexcept = delete;
        SCENE_API ~entity_hierarchy();

        entity_hierarchy& operator=(const entity_hierarchy&) = delete;
        entity_hierarchy& operator=(entity_hierarchy&&) noexcept = delete;

        SCENE_API expected<> init(const ecs::type_registry& typeRegistry);

        SCENE_API ecs::entity_registry& get_entity_registry();
        SCENE_API const ecs::entity_registry& get_entity_registry() const;

        SCENE_API expected<> load(cstring_view source, const entity_hierarchy_serialization_context& ctx);

        SCENE_API expected<> load(
            cstring_view source, const property_registry& propertyRegistry, const ecs_serializer::read_config& cfg);

        SCENE_API expected<> load(const data_document& doc,
            const property_registry& propertyRegistry,
            const ecs_serializer::read_config& cfg);

        SCENE_API expected<> save(cstring_view source, const entity_hierarchy_serialization_context& ctx) const;

        SCENE_API expected<> save(cstring_view source,
            const property_registry& propertyRegistry,
            const ecs_serializer::write_config& cfg) const;

        SCENE_API expected<> save(data_document& doc,
            const property_registry& propertyRegistry,
            const ecs_serializer::write_config& cfg) const;

        SCENE_API expected<> copy_from(const ecs::entity_registry& other,
            const property_registry& propertyRegistry,
            const ecs_serializer::write_config& wCfg,
            const ecs_serializer::read_config& rCfg);

        SCENE_API expected<> copy_to(ecs::entity_registry& other,
            const property_registry& propertyRegistry,
            const ecs_serializer::write_config& wCfg,
            const ecs_serializer::read_config& rCfg);

    private:
        ecs::entity_registry m_registry;
    } OBLO_RESOURCE();
}