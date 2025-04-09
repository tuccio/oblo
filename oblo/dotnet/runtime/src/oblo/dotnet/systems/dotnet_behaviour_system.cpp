#include <oblo/dotnet/systems/dotnet_behaviour_system.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/dotnet/components/dotnet_behaviour_component.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/log/log.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>

namespace oblo
{
    void dotnet_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();

        update(ctx);
    }

    void dotnet_behaviour_system::update(const ecs::system_update_context&) {}

    void dotnet_behaviour_system::shutdown() {}
}