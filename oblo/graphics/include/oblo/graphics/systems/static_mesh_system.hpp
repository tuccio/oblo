#pragma once

#include <oblo/core/handle.hpp>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class renderer;
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
        vk::renderer* m_renderer{};
        resource_registry* m_resourceRegistry;
        vk::resource_cache* m_resourceCache;
        h32<vk::draw_buffer> m_transformBuffer{};
        h32<vk::draw_buffer> m_materialsBuffer{};
        h32<vk::draw_buffer> m_entityIdBuffer{};
    };
}