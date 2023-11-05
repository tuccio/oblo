#pragma once

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo
{
    class resource_registry;
}

namespace oblo::vk
{
    class renderer;
}

namespace oblo
{
    class static_mesh_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        vk::renderer* m_renderer{};
        resource_registry* m_resourceRegistry;
    };
}