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

        constexpr hashed_string_view runtimeModule = "oblo_dotnet_rt"_hsv;
        auto* const dotnetModule = dynamic_cast<dotnet_module*>(module_manager::get().find(runtimeModule));

        if (dotnetModule)
        {
            const auto& dotnetRuntime = dotnetModule->get_runtime();

            m_create = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<create_system_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Behaviour.BehaviourSystem, Oblo.Managed",
                "Create");

            m_destroy = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<destroy_system_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Behaviour.BehaviourSystem, Oblo.Managed",
                "Destroy");

            m_update = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<update_system_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Behaviour.BehaviourSystem, Oblo.Managed",
                "Update");

            m_registerBehaviour = dotnetRuntime.load_assembly_delegate<std::remove_pointer_t<register_behaviour_fn>>(
                "managed/Oblo.Managed.dll",
                "Oblo.Behaviour.BehaviourSystem, Oblo.Managed",
                "RegisterBehaviour");

            if (m_create && m_destroy && m_update && m_registerBehaviour)
            {
                m_managedSystem = m_create();
            }
        }

        update(ctx);
    }

    void dotnet_behaviour_system::update(const ecs::system_update_context& ctx)
    {
        if (m_managedSystem)
        {
            ecs::deferred deferred;

            const auto updatedScripts = m_resourceRegistry->get_updated_events<dotnet_assembly>();

            for (auto&& chunk :
                ctx.entities->range<dotnet_behaviour_component>().exclude<dotnet_behaviour_state_component>())
            {
                for (auto&& [e, b] : chunk.zip<ecs::entity, dotnet_behaviour_component>())
                {
                    auto& state = deferred.add<dotnet_behaviour_state_component>(e);

                    state.script = m_resourceRegistry->get_resource(b.script);
                    state.initialized = false;

                    if (!b.script)
                    {
                        continue;
                    }

                    if (!state.script.is_loaded())
                    {
                        state.script.load_start_async();
                        continue;
                    }
                }
            }

            for (auto&& chunk : ctx.entities->range<dotnet_behaviour_component, dotnet_behaviour_state_component>())
            {
                for (auto&& [e, b, state] :
                    chunk.zip<ecs::entity, dotnet_behaviour_component, dotnet_behaviour_state_component>())
                {
                    if (state.script.is_invalidated() || state.script.as_ref() != b.script)
                    {
                        deferred.remove<dotnet_behaviour_state_component>(e);
                        continue;
                    }

                    if (!state.initialized && state.script.is_loaded())
                    {
                        m_registerBehaviour(m_managedSystem,
                            e.value,
                            state.script->assembly.data(),
                            state.script->assembly.size32());

                        state.initialized = true;
                    }
                }
            }

            deferred.apply(*ctx.entities);

            m_update(m_managedSystem, ctx.entities);
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