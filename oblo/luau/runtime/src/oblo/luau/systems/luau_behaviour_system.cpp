#include <oblo/luau/systems/luau_behaviour_system.hpp>

#include <oblo/log/log.hpp>

#include <luacode.h>
#include <lualib.h>

namespace oblo
{
    void luau_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_state = luaL_newstate();
        luaL_openlibs(m_state);
        luaL_sandbox(m_state);

        update(ctx);
    }

    void luau_behaviour_system::update(const ecs::system_update_context&)
    {
        log::debug("Updating Luau");

        const char code[] = "print('Running from luau!')";

        usize byteCodeSize{};
        auto* const byteCode = luau_compile(code, sizeof(code), nullptr, &byteCodeSize);

        if (!byteCode)
        {
            log::error("Failed to compile");
            return;
        }

        if (luau_load(m_state, "foo", byteCode, byteCodeSize, 0) != LUA_OK)
        {
            log::error("Failed to load");
        }
        else
        {
            lua_pcall(m_state, 0, 0, 0);
        }

        free(byteCode);
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