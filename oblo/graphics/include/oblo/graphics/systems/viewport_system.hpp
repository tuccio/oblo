#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/ecs/handles.hpp>

#include <vulkan/vulkan.h>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class renderer;
}

namespace oblo::graphics
{
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
        vk::renderer* m_renderer{nullptr};

        struct render_graph_data;
        flat_dense_map<ecs::entity, render_graph_data> m_renderGraphs;
    };
};
