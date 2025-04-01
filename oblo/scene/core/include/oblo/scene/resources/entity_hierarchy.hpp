#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/ecs/entity_registry.hpp>

namespace oblo
{
    class data_document;

    namespace reflection
    {
        class reflection_registry;
    }

    namespace ecs_serializer
    {
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

        SCENE_API expected<> init();
        SCENE_API void init(const reflection::reflection_registry& reflection);

        SCENE_API ecs::type_registry& get_type_registry();
        SCENE_API const ecs::type_registry& get_type_registry() const;

        SCENE_API ecs::entity_registry& get_entity_registry();
        SCENE_API const ecs::entity_registry& get_entity_registry() const;

        SCENE_API expected<> load(cstring_view source);
        SCENE_API expected<> load(const data_document& doc);

        SCENE_API expected<> save(cstring_view source) const;
        SCENE_API expected<> save(cstring_view source, const ecs_serializer::write_config& cfg) const;
        SCENE_API expected<> save(data_document& doc, const ecs_serializer::write_config& cfg) const;

        SCENE_API expected<> copy_from(const ecs::entity_registry& other, const ecs_serializer::write_config& cfg);

    private:
        ecs::type_registry m_types;
        ecs::entity_registry m_registry{&m_types};
    };
}