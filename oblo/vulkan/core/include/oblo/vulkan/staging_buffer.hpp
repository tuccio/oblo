#pragma once

#include <oblo/core/ring_buffer_tracker.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/gpu_allocator.hpp>

#include <deque>
#include <span>

namespace oblo::vk
{
    class single_queue_engine;
    class stateful_command_buffer;

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

        bool upload(std::span<const std::byte> source, VkBuffer buffer, u32 bufferOffset);

        bool upload(std::span<const std::byte> source,
            VkImage image,
            VkFormat format,
            VkImageLayout initialImageLayout,
            VkImageLayout finalImageLayout,
            u32 width,
            u32 height,
            VkImageSubresourceLayers subresource,
            VkOffset3D imageOffset,
            VkExtent3D imageExtent);

        bool download(VkBuffer buffer, u32 bufferOffset, std::span<std::byte> destination);

        void flush();

        void poll_submissions();

        void wait_all();
        void wait_for_free_space(u32 freeSpace);

        VkCommandBuffer get_active_command_buffer() const;

    private:
        u8 get_next_submit_index() const;

        void free_submissions(u32 timelineId);

        u32 wait_for_timeline(u32 timelineId);

    private:
        static constexpr u32 MaxConcurrentSubmits{8u};

        struct submitted_upload
        {
            u32 timelineId;
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
            VkSemaphore semaphore;
            VkCommandPool commandPool;
            VkCommandBuffer commandBuffers[MaxConcurrentSubmits];
            submitted_upload submittedUploads[MaxConcurrentSubmits];
            u32 nextTimelineId;
        };

        struct pending_copy
        {
            void* dst;
            u32 segmentOffsets[2];
            u32 segmentSizes[2];
            u64 timelineId;
        };

    private:
        impl m_impl{};
        std::deque<pending_copy> m_pendingCopies;
    };
}