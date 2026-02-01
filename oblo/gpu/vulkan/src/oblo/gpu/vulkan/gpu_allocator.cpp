#include <oblo/vulkan/gpu_allocator.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/log/log.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4324 4505)
#endif

#if 0 // Can change this to enable logging from VMA, it's a little verbose though
    #define VMA_DEBUG_LOG(...)                                                                                         \
        {                                                                                                              \
            constexpr auto bufSize = oblo::log::detail::MaxLogMessageLength;                                           \
            char buf[bufSize];                                                                                         \
            sprintf_s(buf, bufSize, __VA_ARGS__);                                                                      \
            oblo::log::debug("[VMA] {}", buf);                                                                         \
        }
#endif

#ifndef NDEBUG
    #define VMA_ASSERT(...) OBLO_ASSERT(__VA_ARGS__)
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace oblo::vk
{
    static_assert(u32(memory_usage::unknown) == VMA_MEMORY_USAGE_UNKNOWN);
    static_assert(u32(memory_usage::cpu_only) == VMA_MEMORY_USAGE_CPU_ONLY);
    static_assert(u32(memory_usage::gpu_only) == VMA_MEMORY_USAGE_GPU_ONLY);
    static_assert(u32(memory_usage::cpu_to_gpu) == VMA_MEMORY_USAGE_CPU_TO_GPU);
    static_assert(u32(memory_usage::gpu_to_cpu) == VMA_MEMORY_USAGE_GPU_TO_CPU);

    gpu_allocator::gpu_allocator(gpu_allocator&& other) noexcept : m_allocator{other.m_allocator}
    {
        other.m_allocator = nullptr;
    }

    gpu_allocator& gpu_allocator::operator=(gpu_allocator&& other) noexcept
    {
        shutdown();
        m_allocator = other.m_allocator;
        other.m_allocator = nullptr;
        return *this;
    }

    gpu_allocator::~gpu_allocator()
    {
        shutdown();
    }

    bool gpu_allocator::init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
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

    void gpu_allocator::shutdown()
    {
        if (m_allocator)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = nullptr;
        }
    }

    VkResult gpu_allocator::create_buffer(const buffer_initializer& initializer, allocated_buffer* outBuffer)
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
            m_objectDebugUtils.set_object_name(get_device(), outBuffer->buffer, initializer.debugLabel.get());
        }

        return result;
    }

    VkResult gpu_allocator::create_image(const image_initializer& initializer, allocated_image* outImage)
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
            m_objectDebugUtils.set_object_name(get_device(), outImage->image, initializer.debugLabel.get());
        }

        return result;
    }

    void gpu_allocator::destroy(const allocated_buffer& buffer)
    {
        vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
    }

    void gpu_allocator::destroy(const allocated_image& image)
    {
        vmaDestroyImage(m_allocator, image.image, image.allocation);
    }

    VmaAllocation gpu_allocator::create_memory(
        VkMemoryRequirements requirements, memory_usage memoryUsage, debug_label debugLabel)
    {
        const VmaAllocationCreateInfo createInfo{
            .usage = VmaMemoryUsage(memoryUsage),
            .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        VmaAllocation allocation{};
        vmaAllocateMemory(m_allocator, &requirements, &createInfo, &allocation, nullptr);

        if (allocation)
        {
            vmaSetAllocationName(m_allocator, allocation, debugLabel.get());
        }

        return allocation;
    }

    void gpu_allocator::destroy_memory(VmaAllocation allocation)
    {
        vmaFreeMemory(m_allocator, allocation);
    }

    VkResult gpu_allocator::bind_image_memory(VkImage image, VmaAllocation allocation, VkDeviceSize offset)
    {
        return vmaBindImageMemory2(m_allocator, allocation, offset, image, nullptr);
    }

    VkResult gpu_allocator::map(VmaAllocation allocation, void** outMemoryPtr)
    {
        return vmaMapMemory(m_allocator, allocation, outMemoryPtr);
    }

    void gpu_allocator::unmap(VmaAllocation allocation)
    {
        vmaUnmapMemory(m_allocator, allocation);
    }

    VkDevice gpu_allocator::get_device() const
    {
        return m_allocator->m_hDevice;
    }

    VkResult gpu_allocator::invalidate_mapped_memory_ranges(std::span<const VmaAllocation> allocations)
    {
        return vmaInvalidateAllocations(m_allocator, u32(allocations.size()), allocations.data(), nullptr, nullptr);
    }

    const VkAllocationCallbacks* gpu_allocator::get_allocation_callbacks() const
    {
        return m_allocator->GetAllocationCallbacks();
    }

    debug_utils::object gpu_allocator::get_object_debug_utils() const
    {
        return m_objectDebugUtils;
    }

    void gpu_allocator::set_object_debug_utils(debug_utils::object objectUtils)
    {
        m_objectDebugUtils = objectUtils;
    }
}