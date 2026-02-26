#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/ecs/forward.hpp>
#include <oblo/ecs/utility/entity_map.hpp>

namespace oblo
{
    class renderer;
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
        renderer* m_renderer{};
        scene_renderer* m_sceneRenderer{};
        u32 m_frameIndex{};

        ecs::entity_map<render_graph_data> m_renderGraphs;
    };
};
