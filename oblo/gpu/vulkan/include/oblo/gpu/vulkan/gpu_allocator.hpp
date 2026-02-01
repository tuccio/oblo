#pragma once

#include <oblo/core/string/debug_label.hpp>
#include <oblo/core/types.hpp>
#include <oblo/gpu/vulkan/utility/debug_utils.hpp>
#include <vulkan/vulkan.h>

#include <span>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace oblo::gpu::vk
{
    enum class allocated_memory_usage : u8
    {
        unknown = 0,
        gpu_only = 1,
        cpu_only = 2,
        cpu_to_gpu = 3,
        gpu_to_cpu = 4,
    };

    struct allocated_image;
    struct allocated_buffer;

    struct allocated_buffer_initializer;
    struct allocated_image_initializer;

    class gpu_allocator
    {
    public:
        gpu_allocator() = default;
        gpu_allocator(const gpu_allocator&) = delete;

        gpu_allocator(gpu_allocator&&) noexcept;

        gpu_allocator& operator=(const gpu_allocator&) = delete;
        gpu_allocator& operator=(gpu_allocator&&) noexcept;

        ~gpu_allocator();

        VkResult init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

        void shutdown();

        VkResult create_buffer(const allocated_buffer_initializer& initializer, allocated_buffer* outBuffer);
        VkResult create_image(const allocated_image_initializer& initializer, allocated_image* outImage);

        void destroy(const allocated_buffer& buffer);
        void destroy(const allocated_image& image);

        VmaAllocation create_memory(VkMemoryRequirements requirements,
            allocated_memory_usage memoryUsage,
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

    struct allocated_buffer_initializer
    {
        u32 size;
        VkBufferUsageFlags usage;

        // One between required flags and memory usage has to be non-zero
        VkMemoryPropertyFlags requiredFlags;
        allocated_memory_usage memoryUsage;

        debug_label debugLabel{std::source_location::current()};
    };

    struct allocated_image
    {
        VkImage image;
        VmaAllocation allocation;
    };

    struct allocated_image_initializer
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
        allocated_memory_usage memoryUsage;

        debug_label debugLabel{std::source_location::current()};
    };
}