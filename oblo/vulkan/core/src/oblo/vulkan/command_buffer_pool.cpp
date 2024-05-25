#include <oblo/vulkan/command_buffer_pool.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/vulkan/error.hpp>

namespace oblo::vk
{
    command_buffer_pool::command_buffer_pool(command_buffer_pool&& other) noexcept
    {
        m_device = other.m_device;
        m_commandPool = other.m_commandPool;
        m_commandBuffers = std::move(other.m_commandBuffers);
        m_trackingInfo = std::move(other.m_trackingInfo);
        m_resetCommandBuffers = other.m_resetCommandBuffers;

        other.m_device = nullptr;
        other.m_commandPool = nullptr;
    }

    command_buffer_pool& command_buffer_pool::operator=(command_buffer_pool&& other) noexcept
    {
        shutdown();

        m_device = other.m_device;
        m_commandPool = other.m_commandPool;
        m_commandBuffers = std::move(other.m_commandBuffers);
        m_trackingInfo = std::move(other.m_trackingInfo);
        m_resetCommandBuffers = other.m_resetCommandBuffers;

        other.m_device = nullptr;
        other.m_commandPool = nullptr;

        return *this;
    }

    command_buffer_pool::~command_buffer_pool()
    {
        if (m_commandPool)
        {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        }
    }

    bool command_buffer_pool::init(
        VkDevice device, u32 queueFamilyIndex, bool resetCommandBuffers, u32 buffersPerFrame, u32 framesInFlight)
    {
        OBLO_ASSERT(device);

        if (m_commandPool)
        {
            return false;
        }

        const VkCommandPoolCreateInfo commandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = resetCommandBuffers ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0u,
            .queueFamilyIndex = queueFamilyIndex,
        };

        if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        {
            return false;
        }

        m_device = device;
        m_resetCommandBuffers = resetCommandBuffers;

        m_trackingInfo.grow(framesInFlight);
        allocate_buffers(framesInFlight * buffersPerFrame);

        return true;
    }

    void command_buffer_pool::shutdown()
    {
        if (m_commandPool)
        {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = nullptr;
        }

        m_commandBuffers.reset();
        m_trackingInfo.reset();

        m_device = nullptr;
    }

    void command_buffer_pool::begin_frame(u64 frameIndex)
    {
        const auto fetchResult = m_trackingInfo.fetch(1);

        auto* ptr = fetchResult.firstSegmentBegin != fetchResult.firstSegmentEnd ? fetchResult.firstSegmentBegin
                                                                                 : fetchResult.secondSegmentBegin;

        *ptr = tracking_info{.frameIndex = frameIndex, .count = 0};
    }

    void command_buffer_pool::reset_buffers(u64 frameIndex)
    {
        std::size_t freedFrames = 0;
        std::size_t freedBuffers = 0;

        const auto usedTrackingSegments = m_trackingInfo.used_segments();

        auto collect = [&freedFrames, &freedBuffers, frameIndex](tracking_info* begin, tracking_info* end)
        {
            for (auto* it = begin; it != end; ++it)
            {
                if (frameIndex <= it->frameIndex)
                {
                    return false;
                }

                ++freedFrames;
                freedBuffers += it->count;
            }

            return true;
        };

        if (collect(usedTrackingSegments.firstSegmentBegin, usedTrackingSegments.firstSegmentEnd))
        {
            collect(usedTrackingSegments.secondSegmentBegin, usedTrackingSegments.secondSegmentEnd);
        }

        m_trackingInfo.release(freedFrames);

        if (freedBuffers > 0)
        {
            if (m_resetCommandBuffers)
            {
                const auto usedBuffers = m_commandBuffers.used_segments(freedBuffers);

                for (auto it = usedBuffers.firstSegmentBegin; it != usedBuffers.firstSegmentEnd; ++it)
                {
                    OBLO_VK_PANIC(vkResetCommandBuffer(*it, 0u));
                }

                for (auto it = usedBuffers.secondSegmentBegin; it != usedBuffers.secondSegmentEnd; ++it)
                {
                    OBLO_VK_PANIC(vkResetCommandBuffer(*it, 0u));
                }
            }

            m_commandBuffers.release(freedBuffers);
        }
    }

    void command_buffer_pool::reset_pool()
    {
        OBLO_ASSERT(m_commandBuffers.available_count() == m_commandBuffers.capacity());
        OBLO_VK_PANIC(vkResetCommandPool(m_device, m_commandPool, 0u));
    }

    VkCommandBuffer command_buffer_pool::fetch_buffer()
    {
        VkCommandBuffer commandBuffer;
        fetch_buffers({&commandBuffer, 1});
        return commandBuffer;
    }

    void command_buffer_pool::fetch_buffers(std::span<VkCommandBuffer> outBuffers)
    {
        const auto fetchedBuffersCount{outBuffers.size()};

        // TODO: Growth?
        OBLO_ASSERT(m_commandBuffers.has_available(fetchedBuffersCount));

        const auto fetchResult = m_commandBuffers.fetch(fetchedBuffersCount);

        const auto nextIt = std::copy(fetchResult.firstSegmentBegin, fetchResult.firstSegmentEnd, outBuffers.begin());
        std::copy(fetchResult.secondSegmentBegin, fetchResult.secondSegmentEnd, nextIt);

        m_trackingInfo.first_used()->count += fetchedBuffersCount;
    }

    void command_buffer_pool::allocate_buffers(std::size_t count)
    {
        OBLO_ASSERT(m_commandBuffers.capacity() == 0);

        m_commandBuffers.grow(count);

        auto fetchResult = m_commandBuffers.fetch(count);
        OBLO_ASSERT(fetchResult.secondSegmentBegin == fetchResult.secondSegmentEnd);

        const VkCommandBufferAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = u32(count),
        };

        OBLO_VK_PANIC(vkAllocateCommandBuffers(m_device, &allocateInfo, fetchResult.firstSegmentBegin));
        m_commandBuffers.release(count);
    }

    bool command_buffer_pool::is_valid() const
    {
        return m_commandPool != nullptr;
    }
}