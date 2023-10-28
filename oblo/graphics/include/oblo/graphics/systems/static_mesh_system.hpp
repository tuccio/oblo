#pragma once

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::resource
{
    class resource_registry;
}

namespace oblo::vk
{
    class renderer;
}

namespace oblo::graphics
{
    class static_mesh_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        vk::renderer* m_renderer{};
        resource::resource_registry* m_resourceRegistry;
    };
}