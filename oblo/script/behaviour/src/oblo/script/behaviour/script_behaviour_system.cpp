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

        struct script_api_context
        {
            ecs::entity entityId;
        };
    }

    class script_behaviour_system::script_api_impl
    {
    public:
        void register_api_functions(interpreter& i)
        {
            i.register_api("__ecs_set_property", [this](interpreter& interp) { return ecs_set_property_impl(interp); });
        }

        expected<void, interpreter_error> ecs_set_property_impl(interpreter& interp)
        {
            const expected componentType = interp.get_string_view(0);
            const expected property = interp.get_string_view(script_string_ref_size());
            const expected dataRef = interp.get_data_view(2 * script_string_ref_size());

            if (!componentType || !property || !dataRef)
            {
                return interpreter_error::invalid_arguments;
            }

            log::debug("Set {}::{} on entity {} ({} bytes)",
                *componentType,
                *property,
                m_ctx.entityId.value,
                dataRef->size_bytes());

            return no_error;
        }

        void set_context(const script_api_context& ctx)
        {
            m_ctx = ctx;
        }

    private:
        script_api_context m_ctx{};
    };

    script_behaviour_system::script_behaviour_system() = default;
    script_behaviour_system::~script_behaviour_system() = default;

    void script_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_scriptApi = allocate_unique<script_api_impl>();
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

                    m_scriptApi->register_api_functions(*state.runtime);

                    // Assume the whole module is an update function for now
                    // TODO: Handle multiple entry points (e.g. init, update)
                    deferred.add<script_behaviour_update_tag>(e);
                }
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<script_behaviour_state_component>().with<script_behaviour_update_tag>())
        {
            for (auto&& [e, state] : chunk.zip<ecs::entity, script_behaviour_state_component>())
            {
                m_scriptApi->set_context({
                    .entityId = e,
                });

                if (!state.runtime->run())
                {
                    log::debug("Script execution failed for entity {}", e.value);
                }

                state.runtime->reset_execution();
            }
        }
    }

    void script_behaviour_system::shutdown() {}
}