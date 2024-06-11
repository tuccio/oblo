#pragma once

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
        lighting_system() = default;
        lighting_system(const lighting_system&) = delete;
        lighting_system(lighting_system&&) noexcept = delete;

        ~lighting_system() = default;

        lighting_system& operator=(const lighting_system&) = delete;
        lighting_system& operator=(lighting_system&&) noexcept = delete;

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        scene_renderer* m_sceneRenderer{};
    };
};
