#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>

#include <deque>
#include <span>

namespace oblo::vk
{
    class single_queue_engine;
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

        bool init(const single_queue_engine& engine, gpu_allocator& allocator, u32 size);
        void shutdown();

        void begin_frame(u64 frameIndex);
        void end_frame();

        void notify_finished_frames(u64 lastFinishedFrame);

        expected<staging_buffer_span> stage_allocate(u32 size);

        expected<staging_buffer_span> stage(std::span<const std::byte> source);

        expected<staging_buffer_span> stage_image(std::span<const std::byte> source, VkFormat format);

        void copy_to(staging_buffer_span destination, u32 offset, std::span<const std::byte> source);

        void upload(VkCommandBuffer commandBuffer, staging_buffer_span source, VkBuffer buffer, u32 bufferOffset) const;

        void upload(VkCommandBuffer commandBuffer,
            staging_buffer_span source,
            VkImage image,
            VkFormat format,
            VkImageLayout initialImageLayout,
            VkImageLayout finalImageLayout,
            u32 width,
            u32 height,
            VkImageSubresourceLayers subresource,
            VkOffset3D imageOffset,
            VkExtent3D imageExtent);

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
            VkDevice device;
            VkQueue queue;
            u32 queueFamilyIndex;
            gpu_allocator* allocator;
            ring_buffer_tracker<u32> ring;
            u32 pendingBytes;
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