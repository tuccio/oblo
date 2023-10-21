#pragma once

#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    class graph_resources
    {
    public:
        void begin_frame(u64 frameId);
        void end_frame();

    private:
        u64 m_frameId;
        resource_manager m_resources;
    };
}