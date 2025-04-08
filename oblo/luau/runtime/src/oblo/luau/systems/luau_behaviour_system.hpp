#pragma once

#include <oblo/ecs/forward.hpp>

#include <lua.h>

namespace oblo
{
    class resource_registry;

    class luau_behaviour_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);
        void shutdown();

    private:
        lua_State* m_state{};
        const resource_registry* m_resourceRegistry{};
    };
}