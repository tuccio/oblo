#include <oblo/scene/systems/entity_hierarchy_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/components/entity_hierarchy_component.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>

namespace oblo
{
    namespace
    {
        struct entity_hierarchy_loaded
        {
        };

        struct entity_hierarchy_loading
        {
            resource_ptr<entity_hierarchy> hierarchy{};
        };
    }

    void entity_hierarchy_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        auto& typeRegistry = ctx.entities->get_type_registry();

        ecs::register_type<entity_hierarchy_loaded>(typeRegistry);
        ecs::register_type<entity_hierarchy_loading>(typeRegistry);

        update(ctx);
    }

    void entity_hierarchy_system::update(const ecs::system_update_context& ctx)
    {
        ecs::deferred deferred;

        for (auto&& chunk : ctx.entities->range<entity_hierarchy_component>()
                                .exclude<entity_hierarchy_loading, entity_hierarchy_loaded>())
        {
            for (auto&& [e, hc] : chunk.zip<ecs::entity, entity_hierarchy_component>())
            {
                resource_ptr h = m_resourceRegistry->get_resource(hc.hierarchy.id).as<entity_hierarchy>();

                if (!h)
                {
                    // Maybe spawn entity_hierarchy_loading to stop trying?
                    continue;
                }

                h.load_start_async();

                auto& loading = deferred.add<entity_hierarchy_loading>(e);

                loading.hierarchy = std::move(h);
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<entity_hierarchy_loading>().with<entity_hierarchy_component>())
        {
            for (auto&& [e, loading] : chunk.zip<ecs::entity, entity_hierarchy_loading>())
            {
                if (loading.hierarchy.is_loaded())
                {
                    auto* const propertyRegistry = ctx.services->find<property_registry>();
                    OBLO_ASSERT(propertyRegistry);

                    if (!propertyRegistry)
                    {
                        continue;
                    }

                    data_document doc;
                    doc.init();

                    if (!ecs_serializer::write(doc,
                            doc.get_root(),
                            loading.hierarchy->get_entity_registry(),
                            *propertyRegistry)
                             .has_value())
                    {
                        // TODO: Maybe log?
                        continue;
                    }

                    if (!ecs_serializer::read(*ctx.entities, doc, doc.get_root(), *propertyRegistry, e).has_value())
                    {
                        // TODO: Maybe log?
                        continue;
                    }

                    deferred.remove<entity_hierarchy_loading>(e);
                    deferred.add<entity_hierarchy_loaded>(e);

                    // Only process 1 per frame for now
                    deferred.apply(*ctx.entities);

                    return;
                }
            }
        }

        deferred.apply(*ctx.entities);
    }
}