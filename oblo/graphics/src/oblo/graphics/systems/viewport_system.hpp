#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/ecs/forward.hpp>

namespace oblo::vk
{
    class renderer;
}

namespace oblo
{
    class scene_renderer;

    class viewport_system
    {
    public:
        viewport_system();
        viewport_system(const viewport_system&) = delete;
        viewport_system(viewport_system&&) noexcept = delete;

        ~viewport_system();

        viewport_system& operator=(const viewport_system&) = delete;
        viewport_system& operator=(viewport_system&&) noexcept = delete;

        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        struct render_graph_data;

    private:
        vk::renderer* m_renderer{};
        scene_renderer* m_sceneRenderer{};
        u32 m_frameIndex{};

        flat_dense_map<ecs::entity, render_graph_data> m_renderGraphs;
    };
};
