#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

#include <span>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace oblo::vk
{
    enum class memory_usage : u32
    {
        unknown = 0,
        gpu_only = 1,
        cpu_only = 2,
        cpu_to_gpu = 3,
        gpu_to_cpu = 4,
    };

    struct allocated_image;
    struct allocated_buffer;

    struct buffer_initializer;
    struct image_initializer;

    class allocator
    {
    public:
        allocator() = default;
        allocator(const allocator&) = delete;

        allocator(allocator&&) noexcept;

        allocator& operator=(const allocator&) = delete;
        allocator& operator=(allocator&&) noexcept;

        ~allocator();

        bool init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

        void shutdown();

        VkResult create_buffer(const buffer_initializer& initializer, allocated_buffer* outBuffer);
        VkResult create_image(const image_initializer& initializer, allocated_image* outImage);

        void destroy(const allocated_buffer& buffer);
        void destroy(const allocated_image& image);

        VmaAllocation create_memory(VkMemoryRequirements requirements);
        void destroy_memory(VmaAllocation allocation);

        VkResult bind_image_memory(VkImage image, VmaAllocation allocation, VkDeviceSize offset);

        VkResult map(VmaAllocation allocation, void** outMemoryPtr);
        void unmap(VmaAllocation allocation);

        VkDevice get_device() const;

        VkResult invalidate_mapped_memory_ranges(std::span<const VmaAllocation> allocations);

        const VkAllocationCallbacks* get_allocation_callbacks() const;

    private:
        VmaAllocator m_allocator{nullptr};
    };

    struct allocated_buffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    struct buffer_initializer
    {
        u32 size;
        VkBufferUsageFlags usage;
        memory_usage memoryUsage;
    };

    struct allocated_image
    {
        VkImage image;
        VmaAllocation allocation;
    };

    struct image_initializer
    {
        VkImageCreateFlags flags;
        VkImageType imageType;
        VkFormat format;
        VkExtent3D extent;
        u32 mipLevels;
        u32 arrayLayers;
        VkSampleCountFlagBits samples;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkImageLayout initialLayout;
        memory_usage memoryUsage;
    };
}