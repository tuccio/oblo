#pragma once

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class draw_registry;
    class resource_cache;
    struct draw_buffer;
}

namespace oblo
{
    class resource_registry;

    class static_mesh_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        vk::draw_registry* m_drawRegistry{};
        const resource_registry* m_resourceRegistry;
        vk::resource_cache* m_resourceCache;
    };
}