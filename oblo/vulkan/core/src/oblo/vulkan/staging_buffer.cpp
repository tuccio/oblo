#include <oblo/vulkan/staging_buffer.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
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

    bool staging_buffer::init(const single_queue_engine& engine, gpu_allocator& allocator, u32 size)
    {
        OBLO_ASSERT(!m_impl.buffer, "This instance has to be shutdown explicitly");
        OBLO_ASSERT(size > 0);

        m_impl = {};

        const buffer_initializer initializer{
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
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

        return true;
    }

    void staging_buffer::shutdown()
    {
        if (m_impl.allocation)
        {
            if (m_impl.memoryMap)
            {
                m_impl.allocator->unmap(m_impl.allocation);
            }

            m_impl.allocator->destroy(allocated_buffer{m_impl.buffer, m_impl.allocation});
        }

        m_impl = {};
    }

    void staging_buffer::begin_frame(u64 frameIndex)
    {
        m_impl.nextTimelineId = frameIndex;
    }

    void staging_buffer::end_frame()
    {
        m_impl.pendingBytes = 0;
    }

    void staging_buffer::notify_finished_frames(u64 lastFinishedFrame)
    {
        free_submissions(lastFinishedFrame);
    }

    expected<staging_buffer_span> staging_buffer::stage_allocate(u32 size)
    {
        const auto available = m_impl.ring.available_count();

        if (available < size)
        {
            return unspecified_error{};
        }

        const auto segmentedSpan = m_impl.ring.fetch(size);

        u32 segmentOffset{0u};

        for (const auto& segment : segmentedSpan.segments)
        {
            if (segment.begin != segment.end)
            {
                const auto segmentSize = segment.end - segment.begin;
                segmentOffset += segmentSize;
            }
        }

        m_impl.pendingBytes += size;

        return staging_buffer_span{segmentedSpan};
    }

    expected<staging_buffer_span> staging_buffer::stage(std::span<const std::byte> source)
    {
        auto* const srcPtr = source.data();
        const auto srcSize = narrow_cast<u32>(source.size());

        const auto available = m_impl.ring.available_count();

        if (available < srcSize)
        {
            return unspecified_error{};
        }

        const auto segmentedSpan = m_impl.ring.fetch(srcSize);

        u32 segmentOffset{0u};

        for (const auto& segment : segmentedSpan.segments)
        {
            if (segment.begin != segment.end)
            {
                const auto segmentSize = segment.end - segment.begin;
                std::memcpy(m_impl.memoryMap + segment.begin, srcPtr + segmentOffset, segmentSize);

                segmentOffset += segmentSize;
            }
        }

        m_impl.pendingBytes += srcSize;

        return staging_buffer_span{segmentedSpan};
    }

    expected<staging_buffer_span> staging_buffer::stage_image(std::span<const std::byte> source, VkFormat format)
    {
        auto* const srcPtr = source.data();
        const auto srcSize = narrow_cast<u32>(source.size());

        const auto available = m_impl.ring.available_count();

        if (available < srcSize)
        {
            return unspecified_error{};
        }

        (void) format; // TODO: Use the format to determine the alignment instead
        constexpr u32 maxTexelSize = sizeof(f32) * 4;

        // Segments here should be aligned with the texel size, probably we should also be mindful of not splitting a
        // texel in 2 different segments
        const auto segmentedSpan = m_impl.ring.try_fetch_contiguous_aligned(srcSize, maxTexelSize);

        if (segmentedSpan.segments[0].begin == segmentedSpan.segments[0].end)
        {
            OBLO_ASSERT(false, "Failed to allocate space to upload");
            return unspecified_error{};
        }

        if (auto& secondSegment = segmentedSpan.segments[1]; secondSegment.begin != secondSegment.end)
        {
            // TODO: Rather than failing, try to extend it
            OBLO_ASSERT(false,
                "We don't split the image upload, we could at least make sure we have enough space for it");
            return unspecified_error{};
        }

        const auto segment = segmentedSpan.segments[0];
        std::memcpy(m_impl.memoryMap + segment.begin, srcPtr, srcSize);

        m_impl.pendingBytes += srcSize;

        return staging_buffer_span{segmentedSpan};
    }

    void staging_buffer::copy_to(staging_buffer_span destination, u32 offset, std::span<const std::byte> source)
    {
        // Create a subspan out of the destination
        staging_buffer_span subspan = destination;
        subspan.segments[0].begin += offset;

        if (subspan.segments[0].begin > subspan.segments[0].end)
        {
            subspan.segments[0].begin = subspan.segments[0].end;
            subspan.segments[1].begin += subspan.segments[0].end - subspan.segments[0].begin;

            if (subspan.segments[1].begin > subspan.segments[1].end)
            {
                subspan.segments[1].begin = subspan.segments[1].end;
            }
        }

        // Finally try to copy
        u32 remaining = u32(source.size());

        for (u32 segmentIndex = 0; segmentIndex < 2 && remaining > 0; ++segmentIndex)
        {
            const auto& segment = subspan.segments[segmentIndex];

            const auto segmentSize = segment.end - segment.begin;

            if (segmentSize > 0)
            {
                const auto segmentCopySize = min(segmentSize, remaining);
                std::memcpy(m_impl.memoryMap + segment.begin, source.data(), segmentCopySize);
                remaining -= segmentCopySize;
            }
        }

        OBLO_ASSERT(remaining == 0);
    }

    void staging_buffer::upload(
        VkCommandBuffer commandBuffer, staging_buffer_span source, VkBuffer buffer, u32 bufferOffset) const
    {
        VkBufferCopy copyRegions[2];
        u32 regionsCount{0u};

        u32 segmentOffset{0u};

        for (const auto& segment : source.segments)
        {
            if (segment.begin != segment.end)
            {
                const auto segmentSize = segment.end - segment.begin;

                copyRegions[regionsCount] = {
                    .srcOffset = segment.begin,
                    .dstOffset = bufferOffset + segmentOffset,
                    .size = segmentSize,
                };

                segmentOffset += segmentSize;
                ++regionsCount;
            }
        }

        vkCmdCopyBuffer(commandBuffer, m_impl.buffer, buffer, regionsCount, copyRegions);
    }

    void staging_buffer::upload(VkCommandBuffer commandBuffer,
        staging_buffer_span source,
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
        OBLO_ASSERT(calculate_size(source) > 0)
        OBLO_ASSERT(source.segments[1].begin == source.segments[1].end, "Images need contiguous memory");

        const auto& segment = source.segments[0];

        const VkBufferImageCopy copyRegion{
            .bufferOffset = segment.begin,
            .bufferRowLength = width,
            .bufferImageHeight = height,
            .imageSubresource = subresource,
            .imageOffset = imageOffset,
            .imageExtent = imageExtent,
        };

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

        if (finalImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            add_pipeline_barrier_cmd(commandBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                finalImageLayout,
                image,
                format,
                pipelineRange);
        }
    }

    void staging_buffer::free_submissions(u64 timelineId)
    {
        while (!m_impl.submittedUploads.empty())
        {
            const auto& first = m_impl.submittedUploads.front();

            if (timelineId < first.timelineId)
            {
                return;
            }

            m_impl.submittedUploads.pop_front();
        }
    }

    u32 calculate_size(const staging_buffer_span& span)
    {
        return (span.segments[0].end - span.segments[0].begin) + (span.segments[1].end - span.segments[1].begin);
    }
}
