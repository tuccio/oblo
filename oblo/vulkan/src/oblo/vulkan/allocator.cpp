#include <oblo/vulkan/allocator.hpp>

#include <oblo/core/debug.hpp>

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#pragma warning(pop)

namespace oblo::vk
{
    static_assert(u32(memory_usage::unknown) == VMA_MEMORY_USAGE_UNKNOWN);
    static_assert(u32(memory_usage::cpu_only) == VMA_MEMORY_USAGE_CPU_ONLY);
    static_assert(u32(memory_usage::gpu_only) == VMA_MEMORY_USAGE_GPU_ONLY);
    static_assert(u32(memory_usage::cpu_to_gpu) == VMA_MEMORY_USAGE_CPU_TO_GPU);
    static_assert(u32(memory_usage::gpu_to_cpu) == VMA_MEMORY_USAGE_GPU_TO_CPU);

    allocator::allocator(allocator&& other) noexcept : m_allocator{other.m_allocator}
    {
        other.m_allocator = nullptr;
    }

    allocator& allocator::operator=(allocator&& other) noexcept
    {
        shutdown();
        m_allocator = other.m_allocator;
        other.m_allocator = nullptr;
        return *this;
    }

    allocator::~allocator()
    {
        shutdown();
    }

    bool allocator::init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
    {
        if (m_allocator)
        {
            return false;
        }

        const VmaAllocatorCreateInfo createInfo{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice,
            .device = device,
            .instance = instance,
        };

        return vmaCreateAllocator(&createInfo, &m_allocator) == VK_SUCCESS;
    }

    void allocator::shutdown()
    {
        if (m_allocator)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = nullptr;
        }
    }

    VkResult allocator::create_buffer(const buffer_initializer& initializer, allocated_buffer* outBuffer)
    {
        OBLO_ASSERT(initializer.size != 0);

        const VkBufferCreateInfo bufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = initializer.size,
            .usage = initializer.usage,
        };

        const VmaAllocationCreateInfo allocInfo{.usage = VmaMemoryUsage(initializer.memoryUsage)};

        return vmaCreateBuffer(m_allocator,
                               &bufferCreateInfo,
                               &allocInfo,
                               &outBuffer->buffer,
                               &outBuffer->allocation,
                               nullptr);
    }

    VkResult allocator::create_image(const image_initializer& initializer, allocated_image* outImage)
    {
        const VkImageCreateInfo imageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = initializer.flags,
            .imageType = initializer.imageType,
            .format = initializer.format,
            .extent = initializer.extent,
            .mipLevels = initializer.mipLevels,
            .arrayLayers = initializer.arrayLayers,
            .samples = initializer.samples,
            .tiling = initializer.tiling,
            .usage = initializer.usage,
            .initialLayout = initializer.initialLayout,
        };

        const VmaAllocationCreateInfo allocInfo{.usage = VmaMemoryUsage(initializer.memoryUsage)};

        return vmaCreateImage(m_allocator,
                              &imageCreateInfo,
                              &allocInfo,
                              &outImage->image,
                              &outImage->allocation,
                              nullptr);
    }

    void allocator::destroy(const allocated_buffer& buffer)
    {
        vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
    }

    void allocator::destroy(const allocated_image& image)
    {
        vmaDestroyImage(m_allocator, image.image, image.allocation);
    }

    VkResult allocator::map(VmaAllocation allocation, void** outMemoryPtr)
    {
        return vmaMapMemory(m_allocator, allocation, outMemoryPtr);
    }

    void allocator::unmap(VmaAllocation allocation)
    {
        vmaUnmapMemory(m_allocator, allocation);
    }

    VkDevice allocator::get_device() const
    {
        return m_allocator->m_hDevice;
    }
}