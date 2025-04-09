#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class resource_registry;

    class dotnet_behaviour_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);
        void shutdown();

    private:
        const resource_registry* m_resourceRegistry{};
    };
}