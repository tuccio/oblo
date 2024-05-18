#pragma once

#include <oblo/core/debug_label.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/utility/debug_utils.hpp>
#include <vulkan/vulkan.h>

#include <span>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace oblo::vk
{
    enum class memory_usage : u8
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

    class gpu_allocator
    {
    public:
        gpu_allocator() = default;
        gpu_allocator(const gpu_allocator&) = delete;

        gpu_allocator(gpu_allocator&&) noexcept;

        gpu_allocator& operator=(const gpu_allocator&) = delete;
        gpu_allocator& operator=(gpu_allocator&&) noexcept;

        ~gpu_allocator();

        bool init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

        void shutdown();

        VkResult create_buffer(const buffer_initializer& initializer, allocated_buffer* outBuffer);
        VkResult create_image(const image_initializer& initializer, allocated_image* outImage);

        void destroy(const allocated_buffer& buffer);
        void destroy(const allocated_image& image);

        VmaAllocation create_memory(VkMemoryRequirements requirements,
            memory_usage memoryUsage,
            debug_label debugLabel = std::source_location::current());

        void destroy_memory(VmaAllocation allocation);

        VkResult bind_image_memory(VkImage image, VmaAllocation allocation, VkDeviceSize offset);

        VkResult map(VmaAllocation allocation, void** outMemoryPtr);
        void unmap(VmaAllocation allocation);

        VkDevice get_device() const;

        VkResult invalidate_mapped_memory_ranges(std::span<const VmaAllocation> allocations);

        const VkAllocationCallbacks* get_allocation_callbacks() const;

        debug_utils::object get_object_debug_utils() const;
        void set_object_debug_utils(debug_utils::object objectUtils);

    private:
        VmaAllocator m_allocator{nullptr};
        debug_utils::object m_objectDebugUtils{};
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

        // One between required flags and memory usage has to be non-zero
        VkMemoryPropertyFlags requiredFlags;
        memory_usage memoryUsage;

        debug_label debugLabel{std::source_location::current()};
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

        debug_label debugLabel{std::source_location::current()};
    };
}