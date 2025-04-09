#include <oblo/dotnet/systems/dotnet_behaviour_system.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/dotnet/components/dotnet_behaviour_component.hpp>
#include <oblo/dotnet/dotnet_module.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>

namespace oblo
{
    void dotnet_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();

        auto* const dotnetModule = dynamic_cast<dotnet_module*>(module_manager::get().find("oblo_dotnet_rt"_hsv));

        if (dotnetModule)
        {
            const auto& dotnetRuntime = dotnetModule->get_runtime();

            m_create = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<create_system_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Managed.BehaviourSystem, Oblo.Managed",
                "Create");

            m_destroy = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<destroy_system_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Managed.BehaviourSystem, Oblo.Managed",
                "Destroy");

            m_update = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<update_system_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Managed.BehaviourSystem, Oblo.Managed",
                "Update");

            if (m_create && m_destroy && m_update)
            {
                m_managedSystem = m_create();
            }
        }

        update(ctx);
    }

    void dotnet_behaviour_system::update(const ecs::system_update_context&)
    {
        if (m_managedSystem)
        {
            m_update(m_managedSystem);
        }
    }

    void dotnet_behaviour_system::shutdown()
    {
        if (m_managedSystem)
        {
            m_destroy(m_managedSystem);
            m_managedSystem = nullptr;
        }
    }
}