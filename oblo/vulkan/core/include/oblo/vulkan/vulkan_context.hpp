#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>

#include <vector>

namespace oblo::vk
{
    class vulkan_context
    {
    public:
        struct initializer;

    public:
        vulkan_context();
        vulkan_context(const vulkan_context&) = delete;
        vulkan_context(vulkan_context&&) noexcept = delete;
        ~vulkan_context();

        vulkan_context& operator=(const vulkan_context&) = delete;
        vulkan_context& operator=(vulkan_context&&) noexcept = delete;

        bool init(const initializer& init);
        void shutdown();

        void frame_begin();
        void frame_end();

        stateful_command_buffer& get_active_command_buffer();
        void submit_active_command_buffer();

        VkInstance get_instance() const;
        VkDevice get_device() const;
        VkPhysicalDevice get_physical_device() const;

        single_queue_engine& get_engine() const;
        allocator& get_allocator() const;
        resource_manager& get_resource_manager() const;

    private:
        struct submit_info;

    private:
        VkInstance m_instance;
        single_queue_engine* m_engine;
        allocator* m_allocator;
        resource_manager* m_resourceManager;

        std::vector<submit_info> m_submitInfo;

        VkSemaphore m_timelineSemaphore{};

        stateful_command_buffer m_currentCb;

        u32 m_poolIndex{0};
        u64 m_submitIndex{0};
        u64 m_currentSemaphoreValue{0};
    };

    struct vulkan_context::initializer
    {
        VkInstance instance;
        single_queue_engine& engine;
        allocator& allocator;
        resource_manager& resourceManager;
        u32 buffersPerFrame;
        u32 submitsInFlight;
    };

    inline VkInstance vulkan_context::get_instance() const
    {
        return m_instance;
    }

    inline VkDevice vulkan_context::get_device() const
    {
        return m_engine->get_device();
    }

    inline VkPhysicalDevice vulkan_context::get_physical_device() const
    {
        return m_engine->get_physical_device();
    }

    inline single_queue_engine& vulkan_context::get_engine() const
    {
        return *m_engine;
    }

    inline allocator& vulkan_context::get_allocator() const
    {
        return *m_allocator;
    }

    inline resource_manager& vulkan_context::get_resource_manager() const
    {
        return *m_resourceManager;
    }
}