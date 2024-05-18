#include <oblo/vulkan/vulkan_context.hpp>

#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/texture.hpp>

#include <tuple>

namespace oblo::vk
{
    struct vulkan_context::submit_info
    {
        command_buffer_pool pool;
        VkFence fence{VK_NULL_HANDLE};
        u64 submitIndex{0};
    };

    namespace
    {
        template <typename T>
        struct pending_disposal
        {
            T object;
            u64 frameIndex;
        };

        template <>
        struct pending_disposal<VkDescriptorSet>
        {
            VkDescriptorSet object;
            VkDescriptorPool pool;
            u64 frameIndex;
        };
    }

    struct vulkan_context::pending_disposal_queues
    {
    };

    vulkan_context::vulkan_context() = default;

    vulkan_context::~vulkan_context() = default;

    bool vulkan_context::init(const initializer& init)
    {
        OBLO_ASSERT(init.submitsInFlight != 0);
        OBLO_ASSERT(init.buffersPerFrame != 0,
            "This would be ok if we had growth in command_buffer_pool, but we don't currently");

        if (init.submitsInFlight == 0)
        {
            return false;
        }

        m_instance = init.instance;
        m_engine = &init.engine;
        m_allocator = &init.allocator;
        m_resourceManager = &init.resourceManager;

        m_submitInfo.resize(init.submitsInFlight);

        const VkSemaphoreTypeCreateInfo timelineTypeCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0,
        };

        const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &timelineTypeCreateInfo,
            .flags = 0,
        };

        if (vkCreateSemaphore(m_engine->get_device(), &timelineSemaphoreCreateInfo, nullptr, &m_timelineSemaphore) !=
            VK_SUCCESS)
        {
            return false;
        }

        for (auto& submitInfo : m_submitInfo)
        {
            if (!submitInfo.pool
                     .init(m_engine->get_device(), m_engine->get_queue_family_index(), false, init.buffersPerFrame, 1u))
            {
                return false;
            }

            constexpr VkFenceCreateInfo fenceInfo{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0u,
            };

            if (vkCreateFence(m_engine->get_device(), &fenceInfo, nullptr, &submitInfo.fence) != VK_SUCCESS)
            {
                return false;
            }
        }

        m_pending = std::make_unique<pending_disposal_queues>();

        m_debugUtilsLabel = {
            .vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(m_engine->get_device(), "vkCmdBeginDebugUtilsLabelEXT")),
            .vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                vkGetDeviceProcAddr(m_engine->get_device(), "vkCmdEndDebugUtilsLabelEXT")),
        };

        m_debugUtilsObject = {
            .vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                vkGetDeviceProcAddr(m_engine->get_device(), "vkSetDebugUtilsObjectNameEXT")),
        };

        m_allocator->set_object_debug_utils(m_debugUtilsObject);

        return true;
    }

    void vulkan_context::shutdown()
    {
        vkDeviceWaitIdle(m_engine->get_device());

        destroy_resources(~u64{});
        OBLO_ASSERT(m_disposableObjects.empty());

        reset_device_objects(m_engine->get_device(), m_timelineSemaphore);

        for (auto& submitInfo : m_submitInfo)
        {
            reset_device_objects(m_engine->get_device(), submitInfo.fence);
        }

        m_submitInfo.clear();
    }

    void vulkan_context::frame_begin()
    {
        m_poolIndex = u32(m_submitIndex % m_submitInfo.size());

        auto& submitInfo = m_submitInfo[m_poolIndex];

        OBLO_VK_PANIC(
            vkGetSemaphoreCounterValue(m_engine->get_device(), m_timelineSemaphore, &m_currentSemaphoreValue));

        if (m_currentSemaphoreValue < submitInfo.submitIndex)
        {
            OBLO_VK_PANIC(vkWaitForFences(m_engine->get_device(), 1, &submitInfo.fence, 0, UINT64_MAX));
        }

        destroy_resources(m_currentSemaphoreValue);

        OBLO_VK_PANIC(vkResetFences(m_engine->get_device(), 1, &submitInfo.fence));

        auto& pool = submitInfo.pool;

        pool.reset_buffers(m_submitIndex);
        pool.reset_pool();
        pool.begin_frame(m_submitIndex);
    }

    void vulkan_context::frame_end()
    {
        if (m_currentCb.is_valid())
        {
            submit_active_command_buffer();
        }

        ++m_submitIndex;
    }

    stateful_command_buffer& vulkan_context::get_active_command_buffer()
    {
        if (!m_currentCb.is_valid())
        {
            auto& pool = m_submitInfo[m_poolIndex].pool;

            const VkCommandBuffer cb{pool.fetch_buffer()};

            constexpr VkCommandBufferBeginInfo commandBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            OBLO_VK_PANIC(vkBeginCommandBuffer(cb, &commandBufferBeginInfo));

            m_currentCb = stateful_command_buffer{cb};
        }

        return m_currentCb;
    }

    void vulkan_context::submit_active_command_buffer()
    {
        VkCommandBuffer preparationCb{VK_NULL_HANDLE};

        u32 commandBufferBegin = 1;
        constexpr u32 commandBufferEnd = 2;

        auto& currentSubmit = m_submitInfo[m_poolIndex];
        currentSubmit.submitIndex = m_submitIndex;

        if (m_currentCb.has_incomplete_transitions())
        {
            preparationCb = currentSubmit.pool.fetch_buffer();

            constexpr VkCommandBufferBeginInfo commandBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            OBLO_VK_PANIC(vkBeginCommandBuffer(preparationCb, &commandBufferBeginInfo));

            commandBufferBegin = 0;
        }

        VkCommandBuffer commandBuffers[2] = {preparationCb, m_currentCb.get()};

        m_resourceManager->commit(m_currentCb, preparationCb);

        for (u32 i = commandBufferBegin; i < commandBufferEnd; ++i)
        {
            OBLO_VK_PANIC(vkEndCommandBuffer(commandBuffers[i]));
        }

        m_currentCb = {};

        const VkTimelineSemaphoreSubmitInfo timelineInfo{
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = 0,
            .pWaitSemaphoreValues = nullptr,
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues = &m_submitIndex,
        };

        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timelineInfo,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .commandBufferCount = commandBufferEnd - commandBufferBegin,
            .pCommandBuffers = commandBuffers + commandBufferBegin,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &m_timelineSemaphore,
        };

        OBLO_VK_PANIC(vkQueueSubmit(m_engine->get_queue(), 1, &submitInfo, currentSubmit.fence));
    }

    VkPhysicalDeviceSubgroupProperties vulkan_context::get_physical_device_subgroup_properties() const
    {
        VkPhysicalDeviceSubgroupProperties subgroupProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
        };

        VkPhysicalDeviceProperties2 physicalDeviceProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &subgroupProperties,
        };

        vkGetPhysicalDeviceProperties2(get_physical_device(), &physicalDeviceProperties);

        return subgroupProperties;
    }

    void vulkan_context::destroy_immediate(VkBuffer buffer) const
    {
        vkDestroyBuffer(get_device(), buffer, get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_immediate(VmaAllocation allocation) const
    {
        auto& allocator = get_allocator();
        allocator.destroy_memory(allocation);
    }

    void vulkan_context::destroy_immediate(VkSemaphore semaphore) const
    {
        vkDestroySemaphore(get_device(), semaphore, get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_deferred(VkBuffer buffer, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkBuffer buffer) { ctx.destroy_immediate(buffer); },
            buffer);
    }

    void vulkan_context::destroy_deferred(VkImage image, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkImage image)
            { vkDestroyImage(ctx.get_device(), image, ctx.get_allocator().get_allocation_callbacks()); },
            image);
    }

    void vulkan_context::destroy_deferred(VkImageView imageView, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkImageView imageView)
            { vkDestroyImageView(ctx.get_device(), imageView, ctx.get_allocator().get_allocation_callbacks()); },
            imageView);
    }

    void vulkan_context::destroy_deferred(VkDescriptorSet descriptorSet, VkDescriptorPool pool, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkDescriptorSet descriptorSet, VkDescriptorPool pool)
            { vkFreeDescriptorSets(ctx.get_device(), pool, 1, &descriptorSet); },
            descriptorSet,
            pool);
    }

    void vulkan_context::destroy_deferred(VkDescriptorPool pool, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkDescriptorPool pool)
            { vkDestroyDescriptorPool(ctx.get_device(), pool, ctx.get_allocator().get_allocation_callbacks()); },
            pool);
    }

    void vulkan_context::destroy_deferred(VkDescriptorSetLayout setLayout, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkDescriptorSetLayout setLayout) {
                vkDestroyDescriptorSetLayout(ctx.get_device(),
                    setLayout,
                    ctx.get_allocator().get_allocation_callbacks());
            },
            setLayout);
    }

    void vulkan_context::destroy_deferred(VkSampler sampler, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkSampler sampler)
            { vkDestroySampler(ctx.get_device(), sampler, ctx.get_allocator().get_allocation_callbacks()); },
            sampler);
    }

    void vulkan_context::destroy_deferred(VkSemaphore semaphore, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkSemaphore semaphore) { ctx.destroy_immediate(semaphore); },
            semaphore);
    }

    void vulkan_context::destroy_deferred(VmaAllocation allocation, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VmaAllocation allocation) { ctx.destroy_immediate(allocation); },
            allocation);
    }

    void vulkan_context::destroy_deferred(h32<texture> handle, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, h32<texture> handle)
            {
                auto* t = ctx.m_resourceManager->try_find(handle);

                if (!t)
                {
                    return;
                }

                const VkDevice device = ctx.get_device();
                auto& allocator = ctx.get_allocator();
                const VkAllocationCallbacks* const allocationCbs = allocator.get_allocation_callbacks();

                if (auto image = t->image)
                {
                    vkDestroyImage(device, image, allocationCbs);
                }

                if (auto view = t->view)
                {
                    vkDestroyImageView(device, view, allocationCbs);
                }

                if (auto allocation = t->allocation)
                {
                    allocator.destroy_memory(allocation);
                }

                ctx.m_resourceManager->unregister_texture(handle);
            },
            handle);
    }

    debug_utils::label vulkan_context::get_debug_utils_label() const
    {
        return m_debugUtilsLabel;
    }

    debug_utils::object vulkan_context::get_debug_utils_object() const
    {
        return m_debugUtilsObject;
    }

    void vulkan_context::destroy_resources(u64 maxSubmitIndex)
    {
        while (!m_disposableObjects.empty())
        {
            auto& disposableObject = m_disposableObjects.front();

            // Not quite perfect because we don't always insert in order,
            // but it might be good enough for now
            if (disposableObject.submitIndex >= maxSubmitIndex)
            {
                break;
            }

            disposableObject.dispose(*this, disposableObject);
            m_disposableObjects.pop_front();
        }
    }

    void vulkan_context::begin_debug_label(VkCommandBuffer commandBuffer, const char* label) const
    {
        m_debugUtilsLabel.begin(commandBuffer, label);
    }

    void vulkan_context::end_debug_label(VkCommandBuffer commandBuffer) const
    {
        m_debugUtilsLabel.end(commandBuffer);
    }

    template <typename F, typename... T>
    void vulkan_context::dispose(u64 submitIndex, F&& f, T&&... args)
    {
        using A = std::tuple<std::decay_t<T>...>;

        static_assert(sizeof(F) <= sizeof(void*) && alignof(F) <= alignof(void*));
        static_assert(sizeof(A) <= sizeof(disposable_object::buffer) && alignof(A) <= alignof(void*));

        auto& obj = m_disposableObjects.emplace_back();

        obj.dispose = [](vulkan_context& ctx, disposable_object& obj)
        {
            A* const t = reinterpret_cast<A*>(obj.buffer);
            F* const cb = reinterpret_cast<F*>(obj.cb);

            std::apply([&ctx, cb](T&... args) { (*cb)(ctx, args...); }, *t);
            std::destroy_at(t);
        };

        new (obj.cb) F{std::forward<F>(f)};
        new (obj.buffer) A{std::forward<T>(args)...};
        obj.submitIndex = submitIndex;
    }
}