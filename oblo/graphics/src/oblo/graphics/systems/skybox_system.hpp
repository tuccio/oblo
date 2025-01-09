#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo
{
    class resource_registry;
    class scene_renderer;

    class skybox_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        scene_renderer* m_sceneRenderer{};
        resource_registry* m_resourceRegistry{};
    };
};
