#pragma once

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo
{
    class draw_registry;
    class resource_cache;
    struct draw_buffer;

    class resource_registry;

    class static_mesh_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        draw_registry* m_drawRegistry{};
        const resource_registry* m_resourceRegistry;
        resource_cache* m_resourceCache;
    };
}