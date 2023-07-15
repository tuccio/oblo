#pragma once

#include <oblo/vulkan/resource_manager.hpp>

namespace oblo::vk
{
    class stateful_command_buffer : public command_buffer_state
    {
    public:
        stateful_command_buffer() = default;
        explicit stateful_command_buffer(VkCommandBuffer commandBuffer) : m_cmdBuffer{commandBuffer} {}
        stateful_command_buffer(const stateful_command_buffer&) = delete;
        stateful_command_buffer(stateful_command_buffer&&) noexcept = default;
        stateful_command_buffer& operator=(const stateful_command_buffer&) = delete;
        stateful_command_buffer& operator=(stateful_command_buffer&&) noexcept = default;

        using command_buffer_state::set_starting_layout;

        void add_pipeline_barrier(const resource_manager& resourceManager,
                                  h32<texture> handle,
                                  VkImageLayout newLayout)
        {
            command_buffer_state::add_pipeline_barrier(resourceManager, handle, m_cmdBuffer, newLayout);
        }

        VkCommandBuffer get() const
        {
            return m_cmdBuffer;
        }

    private:
        VkCommandBuffer m_cmdBuffer{nullptr};
    };
}