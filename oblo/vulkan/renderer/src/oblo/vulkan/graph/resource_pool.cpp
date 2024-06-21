#include <oblo/vulkan/graph/resource_pool.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    namespace
    {
        VkImageAspectFlags deduce_aspect_mask(VkFormat format)
        {
            switch (format)
            {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;

            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;

            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
            }
        }
    }

    struct resource_pool::texture_resource
    {
        image_initializer initializer;
        lifetime_range range;
        VkImage image;
        VkImageView imageView;
        VkDeviceSize size;
    };

    struct resource_pool::buffer_resource
    {
        u32 size;
        VkBufferUsageFlags usage;
        VkBuffer buffer;
        u32 offset;
    };

    struct resource_pool::buffer_pool
    {
        monotonic_gpu_buffer buffer;
        VkBufferUsageFlags usage;
    };

    resource_pool::resource_pool() = default;

    resource_pool::~resource_pool() = default;

    bool resource_pool::init(vulkan_context& ctx)
    {
        auto& engine = ctx.get_engine();
        const auto physicalDevice = engine.get_physical_device();

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        constexpr u32 bufferChunkSize{1u << 20};

        struct buffer_pool_desc
        {
            VkBufferUsageFlags flags;
            u8 alignment;
        };

        const buffer_pool_desc descs[] = {
            {
                .flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .alignment = narrow_cast<u8>(properties.limits.minUniformBufferOffsetAlignment),
            },
            {
                .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .alignment = narrow_cast<u8>(properties.limits.minStorageBufferOffsetAlignment),
            },
            {
                .flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .alignment = narrow_cast<u8>(
                    properties.limits.minStorageBufferOffsetAlignment), // TODO: Is there a min indirect alignment?
            },
        };

        m_bufferPools.reserve(array_size(descs));

        for (const auto& desc : descs)
        {
            auto& pool = m_bufferPools.emplace_back();
            pool.usage = desc.flags;
            pool.buffer.init(desc.flags, memory_usage::gpu_only, desc.alignment, bufferChunkSize);
        }

        return true;
    }

    void resource_pool::shutdown(vulkan_context& ctx)
    {
        m_lastFrameAllocation = m_allocation;
        std::swap(m_lastFrameTextureResources, m_textureResources);
        free_last_frame_resources(ctx);

        for (auto& pool : m_bufferPools)
        {
            pool.buffer.shutdown(ctx);
        }
    }

    void resource_pool::begin_build()
    {
        m_graphBegin = 0;

        std::swap(m_lastFrameTextureResources, m_textureResources);
        m_textureResources.clear();

        m_lastFrameAllocation = m_allocation;
        m_allocation = nullptr;

        for (auto& pool : m_bufferPools)
        {
            pool.buffer.restore_all();
        }

        m_bufferResources.clear();
    }

    void resource_pool::end_build(vulkan_context& ctx)
    {
        // TODO: Here we should check if we can reuse the allocation from last frame, instead for now we
        // simply free the objects from last frame
        free_last_frame_resources(ctx);

        create_textures(ctx);
        create_buffers(ctx);
    }

    void resource_pool::begin_graph()
    {
        m_graphBegin = u32(m_textureResources.size());
    }

    void resource_pool::end_graph() {}

    u32 resource_pool::add(const image_initializer& initializer, lifetime_range range)
    {
        const auto id = u32(m_textureResources.size());
        m_textureResources.emplace_back(initializer, range);
        return id;
    }

    u32 resource_pool::add_buffer(u32 size, VkBufferUsageFlags usage)
    {
        const auto id = u32(m_bufferResources.size());

        m_bufferResources.push_back({
            .size = size,
            .usage = usage,
        });

        return id;
    }

    void resource_pool::add_usage(u32 poolIndex, VkImageUsageFlags usage)
    {
        m_textureResources[poolIndex].initializer.usage |= usage;
    }

    void resource_pool::add_buffer_usage(u32 poolIndex, VkBufferUsageFlags usage)
    {
        m_bufferResources[poolIndex].usage |= usage;
    }

    texture resource_pool::get_texture(u32 id) const
    {
        auto& resource = m_textureResources[id];

        return {
            .image = resource.image,
            .view = resource.imageView,
            .initializer = resource.initializer,
        };
    }

    buffer resource_pool::get_buffer(u32 id) const
    {
        auto& resource = m_bufferResources[id];

        return {
            .buffer = resource.buffer,
            .offset = resource.offset,
            .size = resource.size,
        };
    }

    void resource_pool::free_last_frame_resources(vulkan_context& ctx)
    {
        const auto submitIndex = ctx.get_submit_index() - 1;

        for (const auto& resource : m_lastFrameTextureResources)
        {
            ctx.destroy_deferred(resource.image, submitIndex);
            ctx.destroy_deferred(resource.imageView, submitIndex);
        }

        if (m_lastFrameAllocation)
        {
            ctx.destroy_deferred(m_lastFrameAllocation, submitIndex);
        }
    }

    void resource_pool::create_textures(vulkan_context& ctx)
    {
        VkDevice device{ctx.get_device()};

        auto& allocator = ctx.get_allocator();
        auto* const allocationCbs = allocator.get_allocation_callbacks();

        VkMemoryRequirements newRequirements{.memoryTypeBits = ~u32{}};

        // For now we just allocate all textures
        for (auto& textureResource : m_textureResources)
        {
            const auto& initializer = textureResource.initializer;
            OBLO_ASSERT(initializer.memoryUsage == memory_usage::gpu_only);

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

            OBLO_VK_PANIC(vkCreateImage(device, &imageCreateInfo, allocationCbs, &textureResource.image));

            VkMemoryRequirements requirements;
            vkGetImageMemoryRequirements(device, textureResource.image, &requirements);

            textureResource.size = requirements.size;

            newRequirements.alignment = max(newRequirements.alignment, requirements.alignment);
            newRequirements.size += requirements.size; // TODO: Actually alias memory
            newRequirements.memoryTypeBits &= requirements.memoryTypeBits;
        }

        // We should maybe constrain memory type bits to use something that uses the bigger heap?
        // https://stackoverflow.com/questions/73243399/vma-how-to-tell-the-library-to-use-the-bigger-of-2-heaps
        OBLO_ASSERT(newRequirements.memoryTypeBits != 0);

        if (newRequirements.size == 0)
        {
            // Nothing to do?
            return;
        }

        // Add space for alignment
        newRequirements.size += (newRequirements.alignment - 1) * m_textureResources.size();

        m_allocation = allocator.create_memory(newRequirements, memory_usage::gpu_only);

        VkDeviceSize offset{0};
        for (auto& textureResource : m_textureResources)
        {
            OBLO_VK_PANIC(allocator.bind_image_memory(textureResource.image, m_allocation, offset));
            offset += textureResource.size + textureResource.size % newRequirements.alignment;

            const VkFormat format{textureResource.initializer.format};

            const VkImageViewCreateInfo imageViewInit{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = textureResource.image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .subresourceRange =
                    {
                        .aspectMask = deduce_aspect_mask(format),
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };

            OBLO_VK_PANIC(vkCreateImageView(device, &imageViewInit, allocationCbs, &textureResource.imageView));
        }
    }

    void resource_pool::create_buffers(vulkan_context& ctx)
    {
        for (auto& buffer : m_bufferResources)
        {
            monotonic_gpu_buffer* poolBuffer{};

            for (auto& pool : m_bufferPools)
            {
                if ((buffer.usage & pool.usage) == buffer.usage)
                {
                    poolBuffer = &pool.buffer;
                    break;
                }
            }

            OBLO_ASSERT(poolBuffer, "Couldn't find compatible pool");

            const auto r = poolBuffer->allocate(ctx, buffer.size);

            buffer.buffer = r.buffer;
            buffer.offset = r.offset;
        }
    }
}