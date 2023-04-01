#pragma once

#include <oblo/core/types.hpp>
#include <vulkan/vulkan.h>

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

    class allocator
    {
    public:
        struct buffer;
        struct buffer_initializer;
        struct image;
        struct image_initializer;

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
        VkResult create_image(const image_initializer& initializer, image* outImage);

        void destroy(const allocator::buffer& buffer);
        void destroy(const allocator::image& image);

        VkResult map(VmaAllocation allocation, void** outMemoryPtr);
        void unmap(VmaAllocation allocation);

        VkDevice get_device() const;

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
        VkBufferUsageFlags usage;
        memory_usage memoryUsage;
    };

    struct allocator::image
    {
        VkImage image;
        VmaAllocation allocation;
    };

    struct allocator::image_initializer
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