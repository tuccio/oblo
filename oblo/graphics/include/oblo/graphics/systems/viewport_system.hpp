#pragma once

#include <vulkan/vulkan.h>

namespace oblo::ecs
{
    struct system_update_context;
}

namespace oblo::vk
{
    class allocator;
    class resource_manager;
}

namespace oblo::graphics
{
    class viewport_system
    {
    public:
        void first_update(const ecs::system_update_context& ctx);
        void update(const ecs::system_update_context& ctx);

    private:
        vk::allocator* m_allocator{nullptr};
        vk::resource_manager* m_resourceManager{nullptr};
        VkSampler m_sampler{};
    };
};
