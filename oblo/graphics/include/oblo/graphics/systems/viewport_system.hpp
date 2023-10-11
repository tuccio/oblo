#pragma once

#include <vulkan/vulkan.h>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class renderer;
    class vulkan_context;
}

namespace oblo::graphics
{
    class viewport_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        vk::vulkan_context* m_vkCtx{nullptr};
        vk::renderer* m_renderer{nullptr};
    };
};
