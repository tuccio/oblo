#include <oblo/vulkan/vulkan_context.hpp>

#include <oblo/core/types.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/command_buffer_pool.hpp>
#include <oblo/vulkan/destroy_device_objects.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/texture.hpp>

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
        std::vector<pending_disposal<VkImage>> images;
        std::vector<pending_disposal<VkImageView>> imageViews;
        std::vector<pending_disposal<VkDescriptorSet>> descriptorSets;
        std::vector<pending_disposal<VkDescriptorPool>> descriptorPools;
        std::vector<pending_disposal<VkDescriptorSetLayout>> descriptorSetLayouts;
        std::vector<pending_disposal<VkSampler>> samplers;
        std::vector<pending_disposal<VmaAllocation>> allocations;
        std::vector<pending_disposal<h32<texture>>> textures;
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

        return true;
    }

    void vulkan_context::shutdown()
    {
        vkDeviceWaitIdle(m_engine->get_device());

        destroy_resources(~u64{});
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

    void vulkan_context::destroy_deferred(VkImage image, u64 submitIndex)
    {
        m_pending->images.emplace_back(image, submitIndex);
    }

    void vulkan_context::destroy_deferred(VkImageView imageView, u64 submitIndex)
    {
        m_pending->imageViews.emplace_back(imageView, submitIndex);
    }

    void vulkan_context::destroy_deferred(VkDescriptorSet descriptorSet, VkDescriptorPool pool, u64 submitIndex)
    {
        m_pending->descriptorSets.emplace_back(descriptorSet, pool, submitIndex);
    }

    void vulkan_context::destroy_deferred(VkDescriptorPool pool, u64 submitIndex)
    {
        m_pending->descriptorPools.emplace_back(pool, submitIndex);
    }

    void vulkan_context::destroy_deferred(VkDescriptorSetLayout setLayout, u64 submitIndex)
    {
        m_pending->descriptorSetLayouts.emplace_back(setLayout, submitIndex);
    }

    void vulkan_context::destroy_deferred(VkSampler sampler, u64 submitIndex)
    {
        m_pending->samplers.emplace_back(sampler, submitIndex);
    }

    void vulkan_context::destroy_deferred(VmaAllocation allocation, u64 submitIndex)
    {
        m_pending->allocations.emplace_back(allocation, submitIndex);
    }

    void vulkan_context::destroy_deferred(h32<texture> texture, u64 submitIndex)
    {
        m_pending->textures.emplace_back(texture, submitIndex);
    }

    void vulkan_context::destroy_resources(u64 maxSubmitIndex)
    {
        auto destroyObjects = [maxSubmitIndex](auto& array, auto doDestroy)
        {
            u32 destroyedObjects{0};

            for (const auto& pending : array)
            {
                // Not quite perfect because we don't always insert in order,
                // but it might be good enough for now
                if (pending.frameIndex >= maxSubmitIndex)
                {
                    break;
                }

                if constexpr (requires { [&] { auto&& [object, frameIndex] = pending; }; })
                {
                    auto&& [object, frameIndex] = pending;
                    doDestroy(object);
                }
                else
                {
                    auto&& [object, secondaryObject, frameIndex] = pending;
                    doDestroy(object, secondaryObject);
                }

                ++destroyedObjects;
            }

            if (destroyedObjects > 0)
            {
                array.erase(array.begin(), array.begin() + destroyedObjects);
            }
        };

        const VkDevice device = get_device();
        auto* const allocationCbs = m_allocator->get_allocation_callbacks();

        destroyObjects(m_pending->images,
            [device, allocationCbs](VkImage image) { vkDestroyImage(device, image, allocationCbs); });

        destroyObjects(m_pending->imageViews,
            [device, allocationCbs](VkImageView imageView) { vkDestroyImageView(device, imageView, allocationCbs); });

        destroyObjects(m_pending->descriptorSets,
            [device](VkDescriptorSet descriptorSet, VkDescriptorPool pool)
            { vkFreeDescriptorSets(device, pool, 1, &descriptorSet); });

        destroyObjects(m_pending->descriptorPools,
            [device, allocationCbs](VkDescriptorPool pool) { vkDestroyDescriptorPool(device, pool, allocationCbs); });

        destroyObjects(m_pending->descriptorSetLayouts,
            [device, allocationCbs](VkDescriptorSetLayout setLayout)
            { vkDestroyDescriptorSetLayout(device, setLayout, allocationCbs); });

        destroyObjects(m_pending->samplers,
            [device, allocationCbs](VkSampler sampler) { vkDestroySampler(device, sampler, allocationCbs); });

        destroyObjects(m_pending->allocations,
            [this](VmaAllocation allocation) { m_allocator->destroy_memory(allocation); });

        destroyObjects(m_pending->textures,
            [this, device, allocationCbs](h32<texture> texture)
            {
                auto* t = m_resourceManager->try_find(texture);

                if (!t)
                {
                    return;
                }

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
                    m_allocator->destroy_memory(allocation);
                }

                m_resourceManager->unregister_texture(texture);
            });
    }
}