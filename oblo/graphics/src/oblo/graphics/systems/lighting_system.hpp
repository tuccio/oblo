#pragma once

#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/ecs/forward.hpp>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class renderer;
}

namespace oblo
{
    class scene_renderer;

    class lighting_system
    {
    public:
        lighting_system();
        lighting_system(const lighting_system&) = delete;
        lighting_system(lighting_system&&) noexcept = delete;

        ~lighting_system();

        lighting_system& operator=(const lighting_system&) = delete;
        lighting_system& operator=(lighting_system&&) noexcept = delete;

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        struct shadow_directional;

    private:
        scene_renderer* m_sceneRenderer{};
        h32_flat_extpool_dense_map<ecs::entity_handle, shadow_directional> m_directionalShadows;
    };
};
