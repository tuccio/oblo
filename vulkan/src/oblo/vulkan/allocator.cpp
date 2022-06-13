#include <oblo/vulkan/allocator.hpp>

#include <oblo/core/debug.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace oblo::vk
{
    static_assert(u32(memory_usage::cpu_to_gpu) == VMA_MEMORY_USAGE_CPU_TO_GPU);

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

        const VmaAllocatorCreateInfo createInfo{.physicalDevice = physicalDevice, .device = device, .instance = instance};
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

    VkResult allocator::create_buffer(const buffer_initializer& initializer, buffer& outBuffer)
    {
        OBLO_ASSERT(initializer.size != 0);

        const VkBufferCreateInfo bufferCreateInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                  .size = initializer.size,
                                                  .usage = initializer.usage};

        const VmaAllocationCreateInfo allocInfo{.usage = VmaMemoryUsage(initializer.memoryUsage)};

        return vmaCreateBuffer(m_allocator,
                               &bufferCreateInfo,
                               &allocInfo,
                               &outBuffer.buffer,
                               &outBuffer.allocation,
                               nullptr);
    }

    void allocator::destroy(const allocator::buffer& buffer)
    {
        vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
    }

    VkResult allocator::map(VmaAllocation allocation, void** outMemoryPtr)
    {
        return vmaMapMemory(m_allocator, allocation, outMemoryPtr);
    }

    void allocator::unmap(VmaAllocation allocation)
    {
        vmaUnmapMemory(m_allocator, allocation);
    }
}