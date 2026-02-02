#include <oblo/vulkan/vulkan_context.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/trace/profile.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/loaded_functions.hpp>
#include <oblo/vulkan/texture.hpp>

#include <tuple>

namespace oblo::vk
{
    struct vulkan_context::frame_info
    {
        command_buffer_pool pool;
        VkFence fence{VK_NULL_HANDLE};
        u64 submitIndex{0};

        // Semaphore to wait on for the first submission, externally owned
        dynamic_array<VkSemaphore> waitSemaphores;
        // Semaphore to signal for the first submission, externally owned
        VkSemaphore signalSemaphore{VK_NULL_HANDLE};
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

        m_frameInfo.resize(init.submitsInFlight);

        m_debugUtilsLabel = {
            .vkCmdBeginDebugUtilsLabelEXT = PFN_vkCmdBeginDebugUtilsLabelEXT(
                vkGetDeviceProcAddr(m_engine->get_device(), "vkCmdBeginDebugUtilsLabelEXT")),
            .vkCmdEndDebugUtilsLabelEXT = PFN_vkCmdEndDebugUtilsLabelEXT(
                vkGetDeviceProcAddr(m_engine->get_device(), "vkCmdEndDebugUtilsLabelEXT")),
        };

        m_debugUtilsObject = {
            .vkSetDebugUtilsObjectNameEXT = PFN_vkSetDebugUtilsObjectNameEXT(
                vkGetDeviceProcAddr(m_engine->get_device(), "vkSetDebugUtilsObjectNameEXT")),
        };

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

        m_debugUtilsObject.set_object_name(m_engine->get_device(),
            m_timelineSemaphore,
            OBLO_STRINGIZE(vulkan_context::m_timelineSemaphore));

        for (auto& submitInfo : m_frameInfo)
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

            m_debugUtilsObject.set_object_name(m_engine->get_device(),
                submitInfo.fence,
                OBLO_STRINGIZE(vulkan_context::frame_info::fence));
        }

#define OBLO_VK_LOAD_FN(name) .name = PFN_##name(vkGetInstanceProcAddr(m_instance, #name))

        m_loadedFunctions = {
            OBLO_VK_LOAD_FN(vkCmdDrawMeshTasksEXT),
            OBLO_VK_LOAD_FN(vkCmdDrawMeshTasksIndirectEXT),
            OBLO_VK_LOAD_FN(vkCmdDrawMeshTasksIndirectCountEXT),
            OBLO_VK_LOAD_FN(vkCreateAccelerationStructureKHR),
            OBLO_VK_LOAD_FN(vkDestroyAccelerationStructureKHR),
            OBLO_VK_LOAD_FN(vkGetAccelerationStructureBuildSizesKHR),
            OBLO_VK_LOAD_FN(vkCmdBuildAccelerationStructuresKHR),
            OBLO_VK_LOAD_FN(vkGetAccelerationStructureDeviceAddressKHR),
            OBLO_VK_LOAD_FN(vkCreateRayTracingPipelinesKHR),
            OBLO_VK_LOAD_FN(vkGetRayTracingShaderGroupHandlesKHR),
            OBLO_VK_LOAD_FN(vkCmdTraceRaysKHR),
        };

#undef OBLO_VK_LOAD_FN

        m_allocator->set_object_debug_utils(m_debugUtilsObject);

        m_debugUtilsObject.set_object_name(m_engine->get_device(),
            VK_OBJECT_TYPE_QUEUE,
            std::bit_cast<u64>(m_engine->get_queue()),
            OBLO_STRINGIZE(single_queue_engine::m_queue));

        return true;
    }

    void vulkan_context::shutdown()
    {
        if (!m_engine)
        {
            return;
        }

        vkDeviceWaitIdle(m_engine->get_device());

        destroy_resources(~u64{});
        OBLO_ASSERT(m_disposableObjects.empty());

        reset_device_objects(m_engine->get_device(), m_timelineSemaphore);

        for (auto& submitInfo : m_frameInfo)
        {
            reset_device_objects(m_engine->get_device(), submitInfo.fence);
        }

        m_frameInfo.clear();
    }

    void vulkan_context::wait_until_ready()
    {
        OBLO_PROFILE_SCOPE();

        m_poolIndex = u32(m_frameIndex % m_frameInfo.size());

        auto& frameInfo = m_frameInfo[m_poolIndex];

        OBLO_VK_PANIC(
            vkGetSemaphoreCounterValue(m_engine->get_device(), m_timelineSemaphore, &m_currentSemaphoreValue));

        if (m_currentSemaphoreValue < frameInfo.submitIndex)
        {
            OBLO_PROFILE_SCOPE("vkWaitForFences");
            OBLO_VK_PANIC(vkWaitForFences(m_engine->get_device(), 1, &frameInfo.fence, 0, UINT64_MAX));
        }
    }

    void vulkan_context::frame_begin(VkSemaphore signalSemaphore)
    {
        OBLO_PROFILE_SCOPE();

        OBLO_ASSERT(m_poolIndex == u32(m_frameIndex % m_frameInfo.size()), "Missing call to wait_until_ready");

        auto& frameInfo = m_frameInfo[m_poolIndex];

        OBLO_VK_PANIC(
            vkGetSemaphoreCounterValue(m_engine->get_device(), m_timelineSemaphore, &m_currentSemaphoreValue));

        if (m_currentSemaphoreValue < frameInfo.submitIndex)
        {
            OBLO_PROFILE_SCOPE("vkWaitForFences");
            OBLO_VK_PANIC(vkWaitForFences(m_engine->get_device(), 1, &frameInfo.fence, 0, UINT64_MAX));
        }

        frameInfo.waitSemaphores.clear();
        frameInfo.signalSemaphore = signalSemaphore;

        destroy_resources(m_currentSemaphoreValue);

        OBLO_VK_PANIC(vkResetFences(m_engine->get_device(), 1, &frameInfo.fence));

        auto& pool = frameInfo.pool;

        pool.reset_buffers(m_submitIndex);
        pool.reset_pool();
        pool.begin_frame(m_submitIndex);
    }

    void vulkan_context::frame_end()
    {
        OBLO_PROFILE_SCOPE();
        submit_active_command_buffer();
        ++m_frameIndex;
    }

    void vulkan_context::push_frame_wait_semaphores(std::span<const VkSemaphore> waitSemaphores)
    {
        auto& frameInfo = m_frameInfo[m_poolIndex];
        frameInfo.waitSemaphores.append(waitSemaphores.begin(), waitSemaphores.end());
    }

    VkCommandBuffer vulkan_context::get_active_command_buffer()
    {
        if (!m_currentCb)
        {
            auto& pool = m_frameInfo[m_poolIndex].pool;

            const VkCommandBuffer cb{*pool.fetch_buffer()};

            constexpr VkCommandBufferBeginInfo commandBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            OBLO_VK_PANIC(vkBeginCommandBuffer(cb, &commandBufferBeginInfo));

            m_currentCb = cb;
        }

        return m_currentCb;
    }

    void vulkan_context::submit_active_command_buffer()
    {
        u32 commandBufferCount = 0;

        auto& currentFrame = m_frameInfo[m_poolIndex];
        currentFrame.submitIndex = m_submitIndex;

        const VkCommandBuffer commandBuffers[1] = {m_currentCb};

        if (m_currentCb)
        {
            OBLO_VK_PANIC(vkEndCommandBuffer(m_currentCb));
            m_currentCb = {};
            commandBufferCount = 1;
        }

        constexpr u32 maxSignalSemaphoresCount = 2;
        const u32 signalSemaphoreCount = 1 + u32{currentFrame.signalSemaphore != nullptr};

        // We need dummy values for all the signaled semaphores
        const u64 signalSemaphoreValues[maxSignalSemaphoresCount] = {m_submitIndex};

        const VkSemaphore signalSemaphores[maxSignalSemaphoresCount] = {
            m_timelineSemaphore,
            currentFrame.signalSemaphore,
        };

        const VkTimelineSemaphoreSubmitInfo timelineInfo{
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = 0,
            .pWaitSemaphoreValues = nullptr,
            .signalSemaphoreValueCount = signalSemaphoreCount,
            .pSignalSemaphoreValues = signalSemaphoreValues,
        };

        buffered_array<VkPipelineStageFlags, 8> waitStages;
        waitStages.assign(currentFrame.waitSemaphores.size(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        const VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &timelineInfo,
            .waitSemaphoreCount = currentFrame.waitSemaphores.size32(),
            .pWaitSemaphores = currentFrame.waitSemaphores.data(),
            .pWaitDstStageMask = waitStages.data(),
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = signalSemaphoreCount,
            .pSignalSemaphores = signalSemaphores,
        };

        OBLO_VK_PANIC(vkQueueSubmit(m_engine->get_queue(), 1, &submitInfo, currentFrame.fence));

        ++m_submitIndex;

        currentFrame.waitSemaphores.clear();
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

    void vulkan_context::destroy_immediate(VkPipeline pipeline) const
    {
        vkDestroyPipeline(get_device(), pipeline, get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_immediate(VkPipelineLayout pipelineLayout) const
    {
        vkDestroyPipelineLayout(get_device(), pipelineLayout, get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_immediate(VkSemaphore semaphore) const
    {
        vkDestroySemaphore(get_device(), semaphore, get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_immediate(VkShaderModule shaderModule) const
    {
        vkDestroyShaderModule(get_device(), shaderModule, get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_immediate(VkAccelerationStructureKHR accelerationStructure) const
    {
        m_loadedFunctions.vkDestroyAccelerationStructureKHR(get_device(),
            accelerationStructure,
            get_allocator().get_allocation_callbacks());
    }

    void vulkan_context::destroy_deferred(VkBuffer buffer, u64 submitIndex)
    {
        dispose(submitIndex, [](vulkan_context& ctx, VkBuffer buffer) { ctx.destroy_immediate(buffer); }, buffer);
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
            [](vulkan_context& ctx, VkDescriptorSetLayout setLayout)
            {
                vkDestroyDescriptorSetLayout(ctx.get_device(),
                    setLayout,
                    ctx.get_allocator().get_allocation_callbacks());
            },
            setLayout);
    }

    void vulkan_context::destroy_deferred(VkPipeline pipeline, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkPipeline pipeline) { ctx.destroy_immediate(pipeline); },
            pipeline);
    }

    void vulkan_context::destroy_deferred(VkPipelineLayout pipelineLayout, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkPipelineLayout pipelineLayout) { ctx.destroy_immediate(pipelineLayout); },
            pipelineLayout);
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

    void vulkan_context::destroy_deferred(VkShaderModule shaderModule, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkShaderModule shaderModule) { ctx.destroy_immediate(shaderModule); },
            shaderModule);
    }

    void vulkan_context::destroy_deferred(VmaAllocation allocation, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VmaAllocation allocation) { ctx.destroy_immediate(allocation); },
            allocation);
    }

    void vulkan_context::destroy_deferred(VkAccelerationStructureKHR accelerationStructure, u64 submitIndex)
    {
        dispose(
            submitIndex,
            [](vulkan_context& ctx, VkAccelerationStructureKHR accelerationStructure)
            { ctx.destroy_immediate(accelerationStructure); },
            accelerationStructure);
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

    gpu::vk::debug_utils::label vulkan_context::get_debug_utils_label() const
    {
        return m_debugUtilsLabel;
    }

    gpu::vk::debug_utils::object vulkan_context::get_debug_utils_object() const
    {
        return m_debugUtilsObject;
    }

    VkDeviceAddress vulkan_context::get_device_address(VkBuffer buffer) const
    {
        const VkBufferDeviceAddressInfo info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer,
        };

        return vkGetBufferDeviceAddress(get_device(), &info);
    }

    VkDeviceAddress vulkan_context::get_device_address(const buffer& buffer) const
    {
        return get_device_address(buffer.buffer) + buffer.offset;
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