#include <oblo/gpu/staging_buffer.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/gpu/descriptors.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/math/power_of_two.hpp>

#include <numeric>

namespace oblo::gpu
{
    namespace
    {
        constexpr u32 InvalidTimelineId{0u};

        staging_buffer_span make_subspan(const staging_buffer_span& destination, u64 offset)
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

    result<> staging_buffer::init(gpu_instance& gpu, u64 size)
    {
        OBLO_ASSERT(!m_impl.buffer, "This instance has to be shutdown explicitly");
        OBLO_ASSERT(size > 0);

        m_impl = {};

        const buffer_descriptor desc{
            .size = size,
            .memoryFlags = flags{memory_requirement::host_visible},
            .usages = buffer_usage::transfer_source | buffer_usage::transfer_destination | buffer_usage::storage,
        };

        const expected buffer = gpu.create_buffer(desc);

        if (!buffer)
        {
            return buffer.error();
        }

        const device_info deviceInfo = gpu.get_device_info();

        m_impl.gpu = &gpu;
        m_impl.ring.reset(size);
        m_impl.buffer = buffer.value();
        m_impl.optimalBufferCopyOffsetAlignment = narrow_cast<u32>(deviceInfo.optimalBufferCopyOffsetAlignment);

        const expected mappedMemory = gpu.memory_map(m_impl.buffer);

        if (!mappedMemory)
        {
            shutdown();
            return mappedMemory.error();
        }
        else
        {
            m_impl.memoryMap = static_cast<byte*>(mappedMemory.value());
        }

        return no_error;
    }

    void staging_buffer::shutdown()
    {
        if (m_impl.buffer)
        {
            if (m_impl.memoryMap)
            {
                m_impl.gpu->memory_unmap(m_impl.buffer).assert_value();
            }

            m_impl.gpu->destroy_buffer(m_impl.buffer);
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

            OBLO_ASSERT(m_impl.ring.used_count() ==
                std::accumulate(m_impl.submittedUploads.begin(),
                    m_impl.submittedUploads.end(),
                    u64{},
                    [](u64 v, const auto& upload) { return upload.size + v; }))
        }

        m_impl.pendingBytes = 0;

        m_impl.nextTimelineId = InvalidTimelineId;
    }

    void staging_buffer::notify_finished_frames(u64 lastFinishedFrame)
    {
        free_submissions(lastFinishedFrame);
    }

    expected<staging_buffer_span> staging_buffer::stage_allocate(u64 size)
    {
        // Workaround for issue #74
        return stage_allocate_contiguous_aligned(size, 1);
    }

    expected<staging_buffer_span> staging_buffer::stage_allocate_internal(u64 size)
    {
        if (!m_impl.ring.has_available(size))
        {
            return "Insufficient space in staging buffer ring"_err;
        }

        const auto segmentedSpan = m_impl.ring.fetch(size);
        OBLO_ASSERT(calculate_size({segmentedSpan}) == size);

        m_impl.pendingBytes += size;

        return staging_buffer_span{segmentedSpan};
    }

    expected<staging_buffer_span> staging_buffer::stage_allocate_contiguous_aligned(u64 size, u64 alignment)
    {
        const auto available = m_impl.ring.available_count();

        if (available < size)
        {
            return "Insufficient space in staging buffer for allocation"_err;
        }

        OBLO_ASSERT(is_power_of_two(alignment));

        const auto firstUnused = m_impl.ring.first_unused();
        const auto firstAligned = align_power_of_two(firstUnused, alignment);
        const auto padding = firstAligned - firstUnused;
        const auto availableFirstSegment = m_impl.ring.first_segment_available_count();

        if (availableFirstSegment >= padding + size)
        {
            stage_allocate_internal(padding).assert_value();
        }
        else if (available - availableFirstSegment >= padding + size)
        {
            stage_allocate_internal(availableFirstSegment).assert_value();
            OBLO_ASSERT(m_impl.ring.first_unused() == 0);
        }
        else
        {
            return "Unable to allocate contiguous aligned space in staging buffer"_err;
        }

        return stage_allocate_internal(size);
    }

    expected<staging_buffer_span> staging_buffer::stage(std::span<const byte> source)
    {
        const usize srcSize = source.size();

        const auto segmentedSpan = stage_allocate(srcSize);

        if (segmentedSpan)
        {
            u64 segmentOffset{0u};

            for (const auto& segment : segmentedSpan->segments)
            {
                if (segment.begin != segment.end)
                {
                    const auto segmentSize = segment.end - segment.begin;
                    std::memcpy(m_impl.memoryMap + segment.begin, source.data() + segmentOffset, segmentSize);

                    segmentOffset += segmentSize;
                }
            }

            OBLO_ASSERT(segmentOffset == srcSize);
        }

        return segmentedSpan;
    }

    expected<staging_buffer_span> staging_buffer::stage_image(std::span<const byte> source, u32 texelSize)
    {
        auto* const srcPtr = source.data();
        const usize srcSize = source.size();

        const u32 alignment = max(m_impl.optimalBufferCopyOffsetAlignment, texelSize);

        const auto result = stage_allocate_contiguous_aligned(srcSize, alignment);

        if (result)
        {
            OBLO_ASSERT(result->segments[1].begin == result->segments[1].end,
                "Images have to be allocated contiguously");

            const auto segment = result->segments[0];
            OBLO_ASSERT(segment.begin % alignment == 0);

            std::memcpy(m_impl.memoryMap + segment.begin, srcPtr, srcSize);
        }

        return result;
    }

    void staging_buffer::copy_to(staging_buffer_span destination, u64 offset, std::span<const byte> source)
    {
        // Create a subspan out of the destination
        const staging_buffer_span subspan = make_subspan(destination, offset);

        // Finally try to copy
        u64 remaining = u32(source.size());
        auto* sourcePtr = source.data();

        for (u64 segmentIndex = 0; segmentIndex < 2 && remaining > 0; ++segmentIndex)
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

    void staging_buffer::copy_from(std::span<byte> dst, staging_buffer_span source, u64 offset)
    {
        // Create a subspan out of the destination
        const staging_buffer_span subspan = make_subspan(source, offset);

        // Finally try to copy
        u64 remaining = dst.size();
        auto* dstPtr = dst.data();

        for (u64 segmentIndex = 0; segmentIndex < 2 && remaining > 0; ++segmentIndex)
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
        hptr<command_buffer> commandBuffer, staging_buffer_span source, h32<buffer> buffer, u64 bufferOffset) const
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);

        OBLO_ASSERT(calculate_size(source) > 0);
        buffer_copy_descriptor copyRegions[2];
        u32 regionsCount{0u};

        u64 segmentOffset{0u};

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

        m_impl.gpu->cmd_copy_buffer(commandBuffer, m_impl.buffer, buffer, {copyRegions, regionsCount});
    }

    void staging_buffer::upload(hptr<command_buffer> commandBuffer,
        h32<image> image,
        std::span<const buffer_image_copy_descriptor> copies) const
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);
        m_impl.gpu->cmd_copy_buffer_to_image(commandBuffer, m_impl.buffer, image, copies);
    }

    void staging_buffer::download(
        hptr<command_buffer> commandBuffer, h32<buffer> buffer, u64 bufferOffset, staging_buffer_span destination) const
    {
        OBLO_ASSERT(m_impl.nextTimelineId != InvalidTimelineId);
        OBLO_ASSERT(calculate_size(destination) > 0);

        buffer_copy_descriptor copyRegions[2];
        u32 regionsCount{0u};

        u64 segmentOffset{0u};

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

        m_impl.gpu->cmd_copy_buffer(commandBuffer, buffer, m_impl.buffer, {copyRegions, regionsCount});
    }

    result<> staging_buffer::invalidate_memory_ranges()
    {
        return m_impl.gpu->memory_invalidate({&m_impl.buffer, 1});
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

    u64 calculate_size(const staging_buffer_span& span)
    {
        return (span.segments[0].end - span.segments[0].begin) + (span.segments[1].end - span.segments[1].begin);
    }
}
