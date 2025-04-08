#include <oblo/luau/systems/luau_behaviour_system.hpp>

#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
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
        void make_table_readonly(lua_State* L)
        {
            OBLO_ASSERT(lua_istable(L, -1));
            lua_setreadonly(L, -1, true);
        }

        void create_log_api(lua_State* L)
        {
            lua_createtable(L, 0, 4);

            {
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int
                    {
                        log::debug("{}", lua_tostring(L, -1));
                        return 0;
                    },
                    "oblo::log::debug");

                lua_rawsetfield(L, -2, "debug");
            }

            {
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int
                    {
                        log::info("{}", lua_tostring(L, -1));
                        return 0;
                    },
                    "oblo::log::info");

                lua_rawsetfield(L, -2, "info");
            }

            {
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int
                    {
                        log::warn("{}", lua_tostring(L, -1));
                        return 0;
                    },
                    "oblo::log::warn");

                lua_rawsetfield(L, -2, "warn");
            }

            {
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int
                    {
                        log::error("{}", lua_tostring(L, -1));
                        return 0;
                    },
                    "oblo::log::error");

                lua_rawsetfield(L, -2, "error");
            }

            make_table_readonly(L);
        }

        void create_ecs_api(lua_State* L, ecs::entity_registry* registry)
        {
            lua_createtable(L, 0, 1);

            {
                lua_pushlightuserdata(L, registry);

                lua_pushcclosure(
                    L,
                    [](lua_State* L) -> int
                    {
                        auto* const reg =
                            static_cast<ecs::entity_registry*>(lua_tolightuserdata(L, lua_upvalueindex(1)));

                        const u32 e = lua_tounsigned(L, -1);
                        const auto result = reg->contains(ecs::entity{e});

                        lua_pushboolean(L, result);

                        return 1;
                    },
                    "oblo::ecs::is_alive",
                    1);

                lua_rawsetfield(L, -2, "is_alive");
            }

            make_table_readonly(L);
        }

        void setup_apis(lua_State* L, const ecs::system_update_context& ctx)
        {
            lua_createtable(L, 0, 1);

            lua_pushvalue(L, -1);
            lua_setglobal(L, "oblo");

            {
                create_ecs_api(L, ctx.entities);
                lua_rawsetfield(L, -2, "ecs");
            }

            {
                create_log_api(L);
                lua_rawsetfield(L, -2, "log");
            }

            {
                lua_getfield(L, -1, "log");
                OBLO_ASSERT(lua_istable(L, -1));

                lua_getfield(L, -1, "info");
                OBLO_ASSERT(lua_isfunction(L, -1));

                lua_setglobal(L, "print");

                OBLO_ASSERT(lua_istable(L, -1));
                lua_pop(L, 1);
            }

            make_table_readonly(L);
            lua_pop(L, 1);
        }
    }

    void luau_behaviour_system::first_update(const ecs::system_update_context& ctx)
    {
        m_resourceRegistry = ctx.services->find<const resource_registry>();

        string_builder sourceCode;

        if (!filesystem::load_text_file_into_memory(sourceCode, "./luau/scripts/luau_behaviour_system.luau"))
        {
            log::error("Failed to load luau_behaviour_system.luau");
            return;
        }

        m_state = luaL_newstate();
        luaL_openlibs(m_state);

        setup_apis(m_state, ctx);

        using bytecode_ptr = unique_ptr<char, decltype([](char* bc) { free(bc); })>;
        bytecode_ptr byteCode;

        usize byteCodeSize;
        byteCode.reset(luau_compile(sourceCode.c_str(), sourceCode.size(), nullptr, &byteCodeSize));

        if (luau_load(m_state, "luau_behaviour_system", byteCode.get(), byteCodeSize, 0) != LUA_OK)
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

        lua_setglobal(m_state, "_system");

        luaL_sandbox(m_state);

        OBLO_ASSERT(lua_gettop(m_state) == 0);

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

        deferred.apply(*ctx.entities);

        for (auto&& chunk : ctx.entities->range<luau_behaviour_component>())
        {
            for (auto&& [e, b] : chunk.zip<ecs::entity, luau_behaviour_component>())
            {
                if (b.initialized && b.scriptPtr && !b.scriptPtr.is_invalidated())
                {
                    continue;
                }

                b.initialized = false;
                b.scriptPtr = m_resourceRegistry->get_resource(b.script);

                if (!b.scriptPtr)
                {
                    continue;
                }

                if (!b.scriptPtr.is_loaded())
                {
                    b.scriptPtr.load_start_async();
                    continue;
                }

                auto& byteCode = b.scriptPtr->byteCode;

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
                    lua_getglobal(m_state, "_system");

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

                    b.initialized = true;
                }
            }
        }

        lua_getglobal(m_state, "_system");
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