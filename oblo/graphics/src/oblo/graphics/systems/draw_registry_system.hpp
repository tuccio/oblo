#pragma once

#include <oblo/ecs/forward.hpp>

namespace oblo
{
    namespace vk
    {
        class draw_registry;
        class vulkan_context;
    }

    class draw_registry_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        bool m_isRayTracingEnabled{};
        vk::vulkan_context* m_vulkanContext{};
        vk::draw_registry* m_drawRegistry{};
    };
}