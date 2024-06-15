#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>

#include <deque>
#include <span>

namespace oblo::vk
{
    class stateful_command_buffer;

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

        bool init(gpu_allocator& allocator, u32 size);
        void shutdown();

        void begin_frame(u64 frameIndex);
        void end_frame();

        void notify_finished_frames(u64 lastFinishedFrame);

        expected<staging_buffer_span> stage_allocate(u32 size);

        expected<staging_buffer_span> stage(std::span<const std::byte> source);

        expected<staging_buffer_span> stage_image(std::span<const std::byte> source, VkFormat format);

        void copy_to(staging_buffer_span destination, u32 destinationOffset, std::span<const std::byte> source);
        void copy_from(std::span<std::byte> destination, staging_buffer_span source, u32 sourceOffset);

        void upload(VkCommandBuffer commandBuffer, staging_buffer_span source, VkBuffer buffer, u32 bufferOffset);

        void upload(VkCommandBuffer commandBuffer,
            staging_buffer_span source,
            VkImage image,
            std::span<const VkBufferImageCopy> copies);

        void download(VkCommandBuffer commandBuffer, VkBuffer buffer, u32 bufferOffset, staging_buffer_span source);

    private:
        void free_submissions(u64 timelineId);

    private:
        struct submitted_upload
        {
            u64 timelineId;
            u32 size;
        };

        struct impl
        {
            gpu_allocator* allocator;
            ring_buffer_tracker<u32> ring;
            u32 pendingBytes;
            u32 transferredBytes;
            VkBuffer buffer;
            VmaAllocation allocation;
            std::byte* memoryMap;
            std::deque<submitted_upload> submittedUploads;
            u64 nextTimelineId;
        };

    private:
        impl m_impl{};
    };

    u32 calculate_size(const staging_buffer_span& span);
}