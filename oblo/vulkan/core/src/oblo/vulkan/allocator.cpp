#include <oblo/vulkan/allocator.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/log.hpp>

#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)

#ifndef NDEBUG
#define VMA_DEBUG_LOG(...)                                                                                             \
    {                                                                                                                  \
        constexpr auto bufSize = oblo::log::detail::MaxLogMessageLength;                                               \
        char buf[bufSize];                                                                                             \
        sprintf_s(buf, bufSize, __VA_ARGS__);                                                                          \
        oblo::log::debug("[VMA] {}", buf);                                                                             \
    }

#endif

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

        const VmaAllocationCreateInfo allocInfo{
            .usage = VmaMemoryUsage(initializer.memoryUsage),
            .requiredFlags = initializer.requiredFlags,
        };

        const auto result = vmaCreateBuffer(m_allocator,
            &bufferCreateInfo,
            &allocInfo,
            &outBuffer->buffer,
            &outBuffer->allocation,
            nullptr);

        if (result == VK_SUCCESS)
        {
            vmaSetAllocationName(m_allocator, outBuffer->allocation, initializer.debugLabel.get());
        }

        return result;
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

        const auto result =
            vmaCreateImage(m_allocator, &imageCreateInfo, &allocInfo, &outImage->image, &outImage->allocation, nullptr);

        if (result == VK_SUCCESS)
        {
            vmaSetAllocationName(m_allocator, outImage->allocation, initializer.debugLabel.get());
        }

        return result;
    }

    void allocator::destroy(const allocated_buffer& buffer)
    {
        vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
    }

    void allocator::destroy(const allocated_image& image)
    {
        vmaDestroyImage(m_allocator, image.image, image.allocation);
    }

    VmaAllocation allocator::create_memory(VkMemoryRequirements requirements, memory_usage memoryUsage)
    {
        const VmaAllocationCreateInfo createInfo{
            .usage = VmaMemoryUsage(memoryUsage),
            .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VmaAllocation allocation{};
        vmaAllocateMemory(m_allocator, &requirements, &createInfo, &allocation, nullptr);
        return allocation;
    }

    void allocator::destroy_memory(VmaAllocation allocation)
    {
        vmaFreeMemory(m_allocator, allocation);
    }

    VkResult allocator::bind_image_memory(VkImage image, VmaAllocation allocation, VkDeviceSize offset)
    {
        return vmaBindImageMemory2(m_allocator, allocation, offset, image, nullptr);
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

    VkResult allocator::invalidate_mapped_memory_ranges(std::span<const VmaAllocation> allocations)
    {
        return vmaInvalidateAllocations(m_allocator, u32(allocations.size()), allocations.data(), nullptr, nullptr);
    }

    const VkAllocationCallbacks* allocator::get_allocation_callbacks() const
    {
        return m_allocator->GetAllocationCallbacks();
    }
}