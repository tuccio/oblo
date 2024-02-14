#pragma once

#include <oblo/core/types.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/instance.hpp>
#include <oblo/vulkan/resource_manager.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>
#include <oblo/vulkan/stateful_command_buffer.hpp>

#include <deque>
#include <memory>
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
        gpu_allocator& get_allocator() const;
        resource_manager& get_resource_manager() const;

        u64 get_submit_index() const;
        bool is_submit_done(u64 submitIndex) const;

        void destroy_deferred(VkBuffer buffer, u64 submitIndex);
        void destroy_deferred(VkImage image, u64 submitIndex);
        void destroy_deferred(VkImageView image, u64 submitIndex);
        void destroy_deferred(VkDescriptorSet descriptorSet, VkDescriptorPool pool, u64 submitIndex);
        void destroy_deferred(VkDescriptorPool pool, u64 submitIndex);
        void destroy_deferred(VkDescriptorSetLayout setLayout, u64 submitIndex);
        void destroy_deferred(VkSampler sampler, u64 submitIndex);
        void destroy_deferred(VmaAllocation allocation, u64 submitIndex);
        void destroy_deferred(h32<texture> texture, u64 submitIndex);

        void begin_debug_label(VkCommandBuffer commandBuffer, const char* label) const;
        void end_debug_label(VkCommandBuffer commandBuffer) const;

    private:
        struct submit_info;

        struct disposable_object
        {
            u64 submitIndex;
            void (*dispose)(vulkan_context& ctx, disposable_object& obj);
            alignas(void*) char cb[sizeof(void*)];
            alignas(void*) char buffer[32];
        };

    private:
        void destroy_resources(u64 maxSubmitIndex);

        template <typename F, typename... T>
        void dispose(u64 submitIndex, F&& f, T&&... args);

    private:
        VkInstance m_instance;
        single_queue_engine* m_engine;
        gpu_allocator* m_allocator;
        resource_manager* m_resourceManager;

        PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelEXT{};
        PFN_vkCmdEndDebugUtilsLabelEXT m_vkCmdEndDebugUtilsLabelEXT{};

        std::vector<submit_info> m_submitInfo;

        VkSemaphore m_timelineSemaphore{};

        stateful_command_buffer m_currentCb;

        u32 m_poolIndex{0};
        u64 m_currentSemaphoreValue{0};

        // We want the submit index to start from more than 0, which is the starting value of the semaphore
        u64 m_submitIndex{1};

        struct pending_disposal_queues;
        std::unique_ptr<pending_disposal_queues> m_pending;

        std::deque<disposable_object> m_disposableObjects;
    };

    struct vulkan_context::initializer
    {
        VkInstance instance;
        single_queue_engine& engine;
        gpu_allocator& allocator;
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

    inline gpu_allocator& vulkan_context::get_allocator() const
    {
        return *m_allocator;
    }

    inline resource_manager& vulkan_context::get_resource_manager() const
    {
        return *m_resourceManager;
    }

    inline u64 vulkan_context::get_submit_index() const
    {
        return m_submitIndex;
    }

    inline bool vulkan_context::is_submit_done(u64 submitIndex) const
    {
        return m_currentSemaphoreValue >= submitIndex;
    }
}