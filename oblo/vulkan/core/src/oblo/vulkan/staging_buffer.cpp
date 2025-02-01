#include <oblo/vulkan/staging_buffer.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>
#include <oblo/vulkan/utility/pipeline_barrier.hpp>

namespace oblo::vk
{
    namespace
    {
        constexpr u32 InvalidTimelineId{0u};

        staging_buffer_span make_subspan(const staging_buffer_span& destination, u32 offset)
        {
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

            return subspan;
        }
    }

    staging_buffer::staging_buffer() = default;

    staging_buffer::~staging_buffer()
    {
        shutdown();
    }

    bool staging_buffer::init(gpu_allocator& allocator, u32 size)
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
        OBLO_ASSERT(frameIndex != InvalidTimelineId);
        OBLO_ASSERT(m_impl.nextTimelineId == InvalidTimelineId);

        m_impl.nextTimelineId = frameIndex;
    }

    void staging_buffer::end_frame()
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);

        // TODO: Maybe some debug check to see segments that have been staged but not uploaded could be useful
        if (m_impl.pendingBytes != 0)
        {
            m_impl.submittedUploads.push_back({.timelineId = m_impl.nextTimelineId, .size = m_impl.pendingBytes});
        }

        m_impl.pendingBytes = 0;

        m_impl.nextTimelineId = InvalidTimelineId;
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
            return unspecified_error;
        }

        const auto segmentedSpan = m_impl.ring.fetch(size);

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
            return unspecified_error;
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
            return unspecified_error;
        }

        (void) format; // TODO: Use the format to determine the alignment instead
        constexpr u32 maxTexelSize = sizeof(f32) * 4;

        // Segments here should be aligned with the texel size, probably we should also be mindful of not splitting a
        // texel in 2 different segments
        const auto segmentedSpan = m_impl.ring.try_fetch_contiguous_aligned(srcSize, maxTexelSize);

        if (segmentedSpan.segments[0].begin == segmentedSpan.segments[0].end)
        {
            OBLO_ASSERT(false, "Failed to allocate space to upload");
            return unspecified_error;
        }

        if (auto& secondSegment = segmentedSpan.segments[1]; secondSegment.begin != secondSegment.end)
        {
            // TODO: Rather than failing, try to extend it
            OBLO_ASSERT(false,
                "We don't split the image upload, we could at least make sure we have enough space for it");
            return unspecified_error;
        }

        const auto segment = segmentedSpan.segments[0];
        std::memcpy(m_impl.memoryMap + segment.begin, srcPtr, srcSize);

        m_impl.pendingBytes += srcSize;

        return staging_buffer_span{segmentedSpan};
    }

    void staging_buffer::copy_to(staging_buffer_span destination, u32 offset, std::span<const std::byte> source)
    {
        // Create a subspan out of the destination
        const staging_buffer_span subspan = make_subspan(destination, offset);

        // Finally try to copy
        u32 remaining = u32(source.size());
        auto* sourcePtr = source.data();

        for (u32 segmentIndex = 0; segmentIndex < 2 && remaining > 0; ++segmentIndex)
        {
            const auto& segment = subspan.segments[segmentIndex];

            const auto segmentSize = segment.end - segment.begin;

            if (segmentSize > 0)
            {
                const auto segmentCopySize = min(segmentSize, remaining);
                std::memcpy(m_impl.memoryMap + segment.begin, sourcePtr, segmentCopySize);
                remaining -= segmentCopySize;
                sourcePtr += segmentCopySize;
            }
        }

        OBLO_ASSERT(remaining == 0);
    }

    void staging_buffer::copy_from(std::span<std::byte> dst, staging_buffer_span source, u32 offset)
    {
        // Create a subspan out of the destination
        const staging_buffer_span subspan = make_subspan(source, offset);

        // Finally try to copy
        u32 remaining = u32(dst.size());
        auto* dstPtr = dst.data();

        for (u32 segmentIndex = 0; segmentIndex < 2 && remaining > 0; ++segmentIndex)
        {
            const auto& segment = subspan.segments[segmentIndex];

            const auto segmentSize = segment.end - segment.begin;

            if (segmentSize > 0)
            {
                const auto segmentCopySize = min(segmentSize, remaining);
                std::memcpy(dstPtr, m_impl.memoryMap + segment.begin, segmentCopySize);
                remaining -= segmentCopySize;
                dstPtr += segmentCopySize;
            }
        }

        OBLO_ASSERT(remaining == 0);
    }

    void staging_buffer::upload(
        VkCommandBuffer commandBuffer, staging_buffer_span source, VkBuffer buffer, u32 bufferOffset) const
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);

        OBLO_ASSERT(calculate_size(source) > 0);
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
        std::span<const VkBufferImageCopy> copies) const
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);

        OBLO_ASSERT(calculate_size(source) > 0)
        OBLO_ASSERT(source.segments[1].begin == source.segments[1].end, "Images need contiguous memory");

        vkCmdCopyBufferToImage(commandBuffer,
            m_impl.buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            u32(copies.size()),
            copies.data());
    }

    void staging_buffer::download(
        VkCommandBuffer commandBuffer, VkBuffer buffer, u32 bufferOffset, staging_buffer_span destination) const
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);
        OBLO_ASSERT(calculate_size(destination) > 0);

        VkBufferCopy copyRegions[2];
        u32 regionsCount{0u};

        u32 segmentOffset{0u};

        for (const auto& segment : destination.segments)
        {
            if (segment.begin != segment.end)
            {
                const auto segmentSize = segment.end - segment.begin;

                copyRegions[regionsCount] = {
                    .srcOffset = bufferOffset + segmentOffset,
                    .dstOffset = segment.begin,
                    .size = segmentSize,
                };

                segmentOffset += segmentSize;
                ++regionsCount;
            }
        }

        vkCmdCopyBuffer(commandBuffer, buffer, m_impl.buffer, regionsCount, copyRegions);
    }

    void staging_buffer::invalidate_memory_ranges()
    {
        OBLO_VK_PANIC(m_impl.allocator->invalidate_mapped_memory_ranges({&m_impl.allocation, 1}));
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

            m_impl.ring.release(first.size);
            m_impl.submittedUploads.pop_front();
        }
    }

    u32 calculate_size(const staging_buffer_span& span)
    {
        return (span.segments[0].end - span.segments[0].begin) + (span.segments[1].end - span.segments[1].begin);
    }
}
