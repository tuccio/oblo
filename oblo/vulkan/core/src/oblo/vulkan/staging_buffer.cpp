#include <oblo/vulkan/staging_buffer.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/allocator.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/pipeline_barrier.hpp>
#include <oblo/vulkan/single_queue_engine.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 InvalidTimelineId{0u};
    }

    staging_buffer::staging_buffer() = default;

    staging_buffer::~staging_buffer()
    {
        shutdown();
    }

    bool staging_buffer::init(const single_queue_engine& engine, allocator& allocator, u32 size)
    {
        OBLO_ASSERT(!m_impl.buffer, "This instance has to be shutdown explicitly");
        OBLO_ASSERT(size > 0);

        m_impl = {};

        const buffer_initializer initializer{
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memoryUsage = memory_usage::cpu_to_gpu,
        };

        allocated_buffer allocatedBuffer;

        if (allocator.create_buffer(initializer, &allocatedBuffer) != VK_SUCCESS)
        {
            return false;
        }

        const VkDevice device = engine.get_device();
        const VkQueue queue = engine.get_queue();
        const u32 queueFamilyIndex = engine.get_queue_family_index();

        m_impl.device = device;
        m_impl.queue = queue;
        m_impl.queueFamilyIndex = queueFamilyIndex;
        m_impl.allocator = &allocator;
        m_impl.ring.reset(size);
        m_impl.buffer = allocatedBuffer.buffer;
        m_impl.allocation = allocatedBuffer.allocation;

        if (void* memoryMap; allocator.map(m_impl.allocation, &memoryMap) != VK_SUCCESS)
        {
            shutdown();
            return false;
        }
        else
        {
            m_impl.memoryMap = static_cast<std::byte*>(memoryMap);
        }

        const VkSemaphoreTypeCreateInfo timelineTypeCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = InvalidTimelineId,
        };

        const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &timelineTypeCreateInfo,
            .flags = 0,
        };

        if (vkCreateSemaphore(device, &timelineSemaphoreCreateInfo, nullptr, &m_impl.semaphore) != VK_SUCCESS)
        {
            shutdown();
            return false;
        }

        const VkCommandPoolCreateInfo commandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = engine.get_queue_family_index(),
        };

        if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &m_impl.commandPool) != VK_SUCCESS)
        {
            shutdown();
            return false;
        }

        const VkCommandBufferAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_impl.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = MaxConcurrentSubmits,
        };

        if (vkAllocateCommandBuffers(device, &allocateInfo, m_impl.commandBuffers) != VK_SUCCESS)
        {
            shutdown();
            return false;
        }

        m_impl.nextTimelineId = InvalidTimelineId + 1;

        const auto nextSubmitIndex = get_next_submit_index();
        const auto commandBuffer = m_impl.commandBuffers[nextSubmitIndex];

        constexpr VkCommandBufferBeginInfo commandBufferBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
        {
            shutdown();
            return false;
        }

        return true;
    }

    void staging_buffer::shutdown()
    {
        if (m_impl.semaphore)
        {
            vkDestroySemaphore(m_impl.device, m_impl.semaphore, nullptr);
        }

        if (m_impl.allocation)
        {
            m_impl.allocator->destroy(allocated_buffer{m_impl.buffer, m_impl.allocation});
        }

        if (m_impl.commandPool)
        {
            vkDestroyCommandPool(m_impl.device, m_impl.commandPool, nullptr);
        }

        m_impl = {};
    }

    bool staging_buffer::upload(std::span<const std::byte> source, VkBuffer buffer, u32 bufferOffset)
    {
        auto* const srcPtr = source.data();
        const auto srcSize = narrow_cast<u32>(source.size());

        const auto available = m_impl.ring.available_count();

        if (available < srcSize)
        {
            return false;
        }

        const auto segmentedSpan = m_impl.ring.fetch(srcSize);

        VkBufferCopy copyRegions[2];
        u32 regionsCount{0u};

        u32 segmentOffset{0u};

        for (const auto& segment : segmentedSpan.segments)
        {
            if (segment.begin != segment.end)
            {
                const auto segmentSize = segment.end - segment.begin;
                std::memcpy(m_impl.memoryMap + segment.begin, srcPtr + segmentOffset, segmentSize);

                copyRegions[regionsCount] = {
                    .srcOffset = segment.begin,
                    .dstOffset = bufferOffset + segmentOffset,
                    .size = segmentSize,
                };

                segmentOffset += segmentSize;
                ++regionsCount;
            }
        }

        const auto nextSubmitIndex = get_next_submit_index();
        const auto commandBuffer = m_impl.commandBuffers[nextSubmitIndex];

        // TODO: Pipeline barriers?
        vkCmdCopyBuffer(commandBuffer, m_impl.buffer, buffer, regionsCount, copyRegions);
        m_impl.pendingUploadBytes += srcSize;

        return true;
    }

    bool staging_buffer::upload(std::span<const std::byte> source,
        VkImage image,
        VkFormat format,
        VkImageLayout initialImageLayout,
        VkImageLayout finalImageLayout,
        u32 width,
        u32 height,
        VkImageSubresourceLayers subresource,
        VkOffset3D imageOffset,
        VkExtent3D imageExtent)
    {
        auto* const srcPtr = source.data();
        const auto srcSize = narrow_cast<u32>(source.size());

        const auto available = m_impl.ring.available_count();

        if (available < srcSize)
        {
            return false;
        }

        const auto segmentedSpan = m_impl.ring.fetch(srcSize);

        if (auto& secondSecment = segmentedSpan.segments[1]; secondSecment.begin != secondSecment.end)
        {
            // TODO: Rather than failing, try to extend it
            OBLO_ASSERT(false,
                "We don't split the image upload, we could at least make sure we have enough space for it");
            return false;
        }

        const auto segment = segmentedSpan.segments[0];
        std::memcpy(m_impl.memoryMap + segment.begin, srcPtr, srcSize);

        const VkBufferImageCopy copyRegion{
            .bufferOffset = segment.begin,
            .bufferRowLength = width,
            .bufferImageHeight = height,
            .imageSubresource = subresource,
            .imageOffset = imageOffset,
            .imageExtent = imageExtent,
        };

        const auto nextSubmitIndex = get_next_submit_index();
        const auto commandBuffer = m_impl.commandBuffers[nextSubmitIndex];

        const VkImageSubresourceRange pipelineRange{
            .aspectMask = subresource.aspectMask,
            .baseMipLevel = subresource.mipLevel,
            .levelCount = 1,
            .baseArrayLayer = subresource.baseArrayLayer,
            .layerCount = 1,
        };

        if (initialImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            add_pipeline_barrier_cmd(commandBuffer,
                initialImageLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                image,
                format,
                pipelineRange);
        }

        vkCmdCopyBufferToImage(commandBuffer,
            m_impl.buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        m_impl.pendingUploadBytes += srcSize;

        if (finalImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            add_pipeline_barrier_cmd(commandBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                finalImageLayout,
                image,
                format,
                pipelineRange);
        }

        return true;
    }

    void staging_buffer::flush()
    {
        if (m_impl.pendingUploadBytes == 0)
        {
            return;
        }

        {
            const auto currentSubmitIndex = get_next_submit_index();

            const auto commandBuffer = m_impl.commandBuffers[currentSubmitIndex];
            OBLO_VK_PANIC(vkEndCommandBuffer(commandBuffer))

            const u64 signalValue{m_impl.nextTimelineId};

            const VkTimelineSemaphoreSubmitInfo timelineInfo{
                .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .waitSemaphoreValueCount = 0,
                .pWaitSemaphoreValues = nullptr,
                .signalSemaphoreValueCount = 1,
                .pSignalSemaphoreValues = &signalValue,
            };

            const VkSubmitInfo submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = &timelineInfo,
                .commandBufferCount = 1u,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &m_impl.semaphore,
            };

            OBLO_VK_PANIC(vkQueueSubmit(m_impl.queue, 1, &submitInfo, nullptr));

            auto& currentSubmit = m_impl.submittedUploads[currentSubmitIndex];
            currentSubmit.timelineId = m_impl.nextTimelineId;
            currentSubmit.size = m_impl.pendingUploadBytes;

            m_impl.pendingUploadBytes = 0;
        }

        poll_submissions();
        ++m_impl.nextTimelineId;

        {
            const auto nextSubmitIndex = get_next_submit_index();
            auto& nextSubmit = m_impl.submittedUploads[nextSubmitIndex];

            if (nextSubmit.timelineId != InvalidTimelineId)
            {
                const u32 actualValue = wait_for_timeline(nextSubmit.timelineId);
                free_submissions(actualValue);
            }

            constexpr VkCommandBufferBeginInfo commandBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            OBLO_VK_PANIC(vkBeginCommandBuffer(m_impl.commandBuffers[nextSubmitIndex], &commandBufferBeginInfo));
        }
    }

    void staging_buffer::poll_submissions()
    {
        u64 value;
        OBLO_VK_PANIC(vkGetSemaphoreCounterValue(m_impl.device, m_impl.semaphore, &value));
        free_submissions(narrow_cast<u32>(value));
    }

    void staging_buffer::wait_for_free_space(u32 freeSpace)
    {
        OBLO_ASSERT(freeSpace <= m_impl.ring.size());

        if (freeSpace <= m_impl.ring.available_count())
        {
            return;
        }

        u32 maxTimelineId{InvalidTimelineId};

        for (u32 i = 1; i <= MaxConcurrentSubmits; ++i)
        {
            const auto nextSubmitIndex = u8((m_impl.nextTimelineId + i) % MaxConcurrentSubmits);
            const auto& nextSubmit = m_impl.submittedUploads[nextSubmitIndex];

            maxTimelineId = max(maxTimelineId, nextSubmit.timelineId);
        }

        OBLO_ASSERT(maxTimelineId != InvalidTimelineId);
        const u32 actualTimelineId = wait_for_timeline(maxTimelineId);
        free_submissions(actualTimelineId);

        OBLO_ASSERT(freeSpace <= m_impl.ring.available_count());
    }

    u8 staging_buffer::get_next_submit_index() const
    {
        return u8((m_impl.nextTimelineId + 1) % MaxConcurrentSubmits);
    }

    void staging_buffer::free_submissions(u32 timelineId)
    {
        for (u32 i = 1; i <= MaxConcurrentSubmits; ++i)
        {
            const auto nextSubmitIndex = u8((m_impl.nextTimelineId + i) % MaxConcurrentSubmits);
            auto& nextSubmit = m_impl.submittedUploads[nextSubmitIndex];

            if (nextSubmit.timelineId == InvalidTimelineId || nextSubmit.timelineId > timelineId)
            {
                continue;
            }

            m_impl.ring.release(nextSubmit.size);

            OBLO_VK_PANIC(vkResetCommandBuffer(m_impl.commandBuffers[nextSubmitIndex], 0u));

            nextSubmit = {};
        }
    }

    u32 staging_buffer::wait_for_timeline(u32 timelineId)
    {
        u64 waitValue{timelineId};

        const VkSemaphoreWaitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1u,
            .pSemaphores = &m_impl.semaphore,
            .pValues = &waitValue,
        };

        OBLO_VK_PANIC(vkWaitSemaphores(m_impl.device, &waitInfo, UINT64_MAX));
        return narrow_cast<u32>(waitValue);
    }
}
