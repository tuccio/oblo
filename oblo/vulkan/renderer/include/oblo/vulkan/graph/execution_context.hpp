#pragma once

#include <oblo/vulkan/graph/pins.hpp>

#include <span>

#include <vulkan/vulkan.h>

namespace oblo::vk
{
    class resource_manager;
    struct texture;
}

namespace oblo::vk
{
    class render_graph;
    struct cpu_data;
    struct gpu_resource;

    class execution_context
    {
        friend class render_graph;

    public:
        template <typename T>
        T& access(data<T> data) const;

        texture access(resource<texture> texture) const;

        VkCommandBuffer get_command_buffer() const;

    private:
        VkCommandBuffer m_commandBuffer{};
        resource_manager* m_graphResources{};
        std::span<const gpu_resource> m_gpuResources;
        std::span<const cpu_data> m_cpuData;
    };
}