#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace oblo::vk
{
    enum class memory_usage : u32
    {
        cpu_to_gpu = 3
    };

    class allocator
    {
    public:
        struct buffer;
        struct buffer_initializer;

    public:
        allocator() = default;
        allocator(const allocator&) = delete;

        allocator(allocator&&) noexcept;

        allocator& operator=(const allocator&) = delete;
        allocator& operator=(allocator&&) noexcept;

        ~allocator();

        bool init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
        void shutdown();

        VkResult create_buffer(const buffer_initializer& initializer, buffer* outBuffer);

        void destroy(const allocator::buffer& buffer);

        VkResult map(VmaAllocation allocation, void** outMemoryPtr);
        void unmap(VmaAllocation allocation);

    private:
        VmaAllocator m_allocator{nullptr};
    };

    struct allocator::buffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    struct allocator::buffer_initializer
    {
        u32 size;
        VkBufferCreateFlags usage;
        memory_usage memoryUsage;
    };
}