#include <oblo/script/behaviour/script_behaviour_system.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/script/behaviour/script_behaviour_component.hpp>
#include <oblo/script/interpreter.hpp>
#include <oblo/script/resources/compiled_script.hpp>

namespace oblo
{
    namespace
    {
        constexpr u32 g_StackSize = 4096;
    }

    void script_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();
        update(ctx);
    }

    void script_behaviour_system::update(const ecs::system_update_context& ctx)
    {
        ecs::deferred deferred;

        const auto updatedScripts = m_resourceRegistry->get_updated_events<compiled_script>();

        for (auto&& chunk :
            ctx.entities->range<script_behaviour_component>().exclude<script_behaviour_state_component>())
        {
            for (auto&& [e, b] : chunk.zip<ecs::entity, script_behaviour_component>())
            {
                auto& state = deferred.add<script_behaviour_state_component>(e);

                state.script = m_resourceRegistry->get_resource(b.script);

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

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<script_behaviour_component, script_behaviour_state_component>())
        {
            for (auto&& [e, b, state] :
                chunk.zip<ecs::entity, script_behaviour_component, script_behaviour_state_component>())
            {
                if (state.script.is_invalidated() || state.script.as_ref() != b.script)
                {
                    deferred.remove<script_behaviour_state_component>(e);
                    continue;
                }

                if (!state.runtime && state.script.is_loaded())
                {
                    // TOOD: Every script owns the stack here, we would like to share memory instead
                    state.runtime = allocate_unique<interpreter>();
                    state.runtime->init(g_StackSize);
                    state.runtime->load_module(state.script->module);

                    // Assume the whole module is an update function for now
                    // TODO: Handle multiple entry points (e.g. init, update)
                    deferred.add<script_behaviour_update_tag>(e);
                }
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<script_behaviour_state_component, script_behaviour_update_tag>())
        {
            for (auto&& [e, state] : chunk.zip<ecs::entity, script_behaviour_state_component>())
            {
                state.runtime->run();
            }
        }
    }

    void script_behaviour_system::shutdown() {}
}