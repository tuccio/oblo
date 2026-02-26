#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    class draw_registry;
    class renderer;

    class draw_registry_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        bool m_isRayTracingEnabled{};
        draw_registry* m_drawRegistry{};
        renderer* m_renderer{};
    };
}