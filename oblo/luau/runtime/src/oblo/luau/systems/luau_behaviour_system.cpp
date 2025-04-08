#include <oblo/luau/systems/luau_behaviour_system.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/utility/deferred.hpp>
#include <oblo/log/log.hpp>
#include <oblo/luau/components/luau_behaviour_component.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>

#include <luacode.h>
#include <lualib.h>

namespace oblo
{
    namespace
    {
        constexpr cstring_view g_luaSystemCode = R"(
print('Starting up!\n')

local m = {
    entities = {},
}

function m:add(e, script)
    print('Calling add!')

    self.entities[#self.entities + 1] = {
        entity = e,
        script = script,
    }
end

function m:update()
    print('Calling update! #entities: ' .. #self.entities)

    for k, v in ipairs(self.entities) do
        if v.script then
            v.userdata = v.script()
            v.script = nil
        end

        if v.userdata then
            v.userdata:update()
        end
    end
end

return m
)";
    }

    void luau_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();

        m_state = luaL_newstate();
        luaL_openlibs(m_state);

        using bytecode_ptr = unique_ptr<char, decltype([](char* bc) { free(bc); })>;
        bytecode_ptr byteCode;

        usize byteCodeSize;
        byteCode.reset(luau_compile(g_luaSystemCode.c_str(), g_luaSystemCode.size(), nullptr, &byteCodeSize));

        if (luau_load(m_state, "_oblo_system", byteCode.get(), byteCodeSize, 0) != LUA_OK)
        {
            log::error("Lua error: {}", lua_tostring(m_state, -1));
            lua_pop(m_state, 1);

            lua_close(m_state);
            m_state = nullptr;
            return;
        }

        if (lua_pcall(m_state, 0, 1, 0) != LUA_OK)
        {
            log::error("Lua error: {}", lua_tostring(m_state, -1));
            lua_pop(m_state, 1);
        }

        lua_setglobal(m_state, "_oblo_system");

        luaL_sandbox(m_state);

        update(ctx);
    }

    void luau_behaviour_system::update(const ecs::system_update_context& ctx)
    {
        if (!m_state) [[unlikely]]
        {
            return;
        }

        ecs::deferred deferred;

        const auto updatedScripts = m_resourceRegistry->get_updated_events<luau_bytecode>();

        if (!updatedScripts.empty())
        {
            for (auto&& chunk : ctx.entities->range<luau_behaviour_component>().with<luau_behaviour_loaded_tag>())
            {
                for (auto&& [e, b] : chunk.zip<ecs::entity, luau_behaviour_component>())
                {
                    const auto it = std::find(updatedScripts.begin(), updatedScripts.end(), b.script.id);

                    if (it != updatedScripts.end())
                    {
                        deferred.remove<luau_behaviour_loaded_tag>(e);
                    }
                }
            }
        }

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<luau_behaviour_component>().exclude<luau_behaviour_loaded_tag>())
        {
            for (auto&& [e, b] : chunk.zip<ecs::entity, luau_behaviour_component>())
            {
                const resource_ptr script = m_resourceRegistry->get_resource(b.script);

                if (!script)
                {
                    continue;
                }

                if (!script.is_loaded())
                {
                    script.load_start_async();
                    continue;
                }

                auto& byteCode = script->byteCode;

                if (luau_load(m_state,
                        "luau_behaviour",
                        reinterpret_cast<const char*>(byteCode.data()),
                        byteCode.size_bytes(),
                        0) != LUA_OK)
                {
                    log::error("Failed to load");
                    log::error("Lua error: {}", lua_tostring(m_state, -1));
                    lua_pop(m_state, 1);
                }
                else
                {
                    lua_getglobal(m_state, "_oblo_system");

                    lua_getfield(m_state, -1, "add");

                    lua_pushvalue(m_state, -2);
                    OBLO_ASSERT(lua_istable(m_state, -1));

                    lua_pushunsigned(m_state, e.value);

                    lua_pushvalue(m_state, -5);
                    OBLO_ASSERT(lua_isfunction(m_state, -1));

                    if (lua_pcall(m_state, 3, 0, 0) != 0)
                    {
                        log::error("Lua error: {}", lua_tostring(m_state, -1));
                        lua_pop(m_state, 1);
                    }

                    lua_pop(m_state, 2);

                    OBLO_ASSERT(lua_gettop(m_state) == 0);

                    deferred.add<luau_behaviour_loaded_tag>(e);
                }
            }
        }

        lua_getglobal(m_state, "_oblo_system");
        lua_getfield(m_state, -1, "update");
        lua_pushvalue(m_state, -2);

        if (lua_pcall(m_state, 1, 0, 0) != LUA_OK)
        {
            log::error("Lua error: {}", lua_tostring(m_state, -1));
            lua_pop(m_state, 1);
        }

        lua_pop(m_state, 1);

        OBLO_ASSERT(lua_gettop(m_state) == 0);

        deferred.apply(*ctx.entities);
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