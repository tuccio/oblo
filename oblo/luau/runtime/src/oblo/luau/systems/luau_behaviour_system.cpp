#include <oblo/luau/systems/luau_behaviour_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/log/log.hpp>
#include <oblo/luau/components/luau_behaviour_component.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>

#include <luacode.h>
#include <lualib.h>

namespace oblo
{
    void luau_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();

        m_state = luaL_newstate();
        luaL_openlibs(m_state);
        luaL_sandbox(m_state);

        update(ctx);
    }

    void luau_behaviour_system::update(const ecs::system_update_context& ctx)
    {
        for (auto&& chunk : ctx.entities->range<luau_behaviour_component>())
        {
            for (auto&& b : chunk.get<luau_behaviour_component>())
            {
                const resource_ptr script = m_resourceRegistry->get_resource(b.script);

                if (!script)
                {
                    continue;
                }

                if (luau_load(m_state,
                        "luau_behaviour",
                        reinterpret_cast<const char*>(script->byteCode.data()),
                        script->byteCode.size_bytes(),
                        0) != LUA_OK)
                {
                    log::error("Failed to load");
                }
                else
                {
                    lua_pcall(m_state, 0, 0, 0);
                }
            }
        }
    }

    void luau_behaviour_system::shutdown()
    {
        if (m_state)
        {
            lua_close(m_state);
            m_state = {};
        }
    }
}