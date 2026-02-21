#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/error.hpp>
#include <oblo/gpu/forward.hpp>

#include <span>

namespace oblo::gpu
{
    struct staging_buffer_span : ring_buffer_tracker<u32>::segmented_span
    {
    };

    class staging_buffer
    {
    public:
        staging_buffer();
        staging_buffer(const staging_buffer&) = delete;
        staging_buffer(staging_buffer&&) noexcept = delete;

        staging_buffer& operator=(const staging_buffer&) = delete;
        staging_buffer& operator=(staging_buffer&&) noexcept = delete;

        ~staging_buffer();

        result<> init(gpu_instance& gpu, u64 size);
        void shutdown();

        void begin_frame(u64 frameIndex);
        void end_frame();

        void notify_finished_frames(u64 lastFinishedFrame);

        expected<staging_buffer_span> stage_allocate(u64 size);

        expected<staging_buffer_span> stage(std::span<const byte> source);

        expected<staging_buffer_span> stage_image(std::span<const byte> source, u32 texelSize);

        void copy_to(staging_buffer_span destination, u32 destinationOffset, std::span<const byte> source);
        void copy_from(std::span<byte> destination, staging_buffer_span source, u32 sourceOffset);

        void upload(
            hptr<command_buffer> commandBuffer, staging_buffer_span source, h32<buffer> buffer, u64 bufferOffset) const;

        void upload(hptr<command_buffer> commandBuffer,
            h32<image> image,
            std::span<const buffer_image_copy_descriptor> copies) const;

        void download(
            hptr<command_buffer> commandBuffer, h32<buffer> buffer, u32 bufferOffset, staging_buffer_span source) const;

        result<> invalidate_memory_ranges();

    private:
        expected<staging_buffer_span> stage_allocate_internal(u64 size);
        expected<staging_buffer_span> stage_allocate_contiguous_aligned(u64 size, u32 alignment);

        void free_submissions(u64 timelineId);

    private:
        struct submitted_upload
        {
            u64 timelineId;
            u64 size;
        };

        struct impl
        {
            gpu_instance* gpu;
            ring_buffer_tracker<u32> ring;
            u32 pendingBytes;
            u32 optimalBufferCopyOffsetAlignment;
            h32<buffer> buffer;
            byte* memoryMap;
            deque<submitted_upload> submittedUploads;
            u64 nextTimelineId;
        };

    private:
        impl m_impl{};
    };

    u32 calculate_size(const staging_buffer_span& span);
}