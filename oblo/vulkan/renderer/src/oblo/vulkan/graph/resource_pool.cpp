#include <oblo/vulkan/graph/resource_pool.hpp>

#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    struct resource_pool::texture_resource
    {
        image_initializer initializer;
        lifetime_range range;
        VkImage image;
        VkDeviceSize size;
    };

    resource_pool::resource_pool() = default;

    resource_pool::~resource_pool() = default;

    void resource_pool::shutdown(vulkan_context& ctx)
    {
        m_lastFrameAllocation = m_allocation;
        std::swap(m_lastFrameTextureResources, m_textureResources);
        free_last_frame_resources(ctx);
    }

    void resource_pool::begin_build()
    {
        m_graphBegin = 0;

        std::swap(m_lastFrameTextureResources, m_textureResources);
        m_textureResources.clear();

        m_lastFrameAllocation = m_allocation;
        m_allocation = nullptr;
    }

    void resource_pool::end_build(vulkan_context& ctx)
    {
        // TODO: Here we should check if we can reuse the allocation from last frame, instead for now we
        // simply free the objects from last frame
        free_last_frame_resources(ctx);

        VkDevice device{ctx.get_device()};

        auto& allocator = ctx.get_allocator();
        auto* const allocationCbs = allocator.get_allocation_callbacks();

        VkMemoryRequirements newRequirements{.memoryTypeBits = ~u32{}};

        // For new we jsut allocate all textures
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

        // TODO: Store it, determine when to free it
        const auto allocation = allocator.create_memory(newRequirements, memory_usage::gpu_only);

        VkDeviceSize offset{0};
        for (const auto& textureResource : m_textureResources)
        {
            OBLO_VK_PANIC(allocator.bind_image_memory(textureResource.image, allocation, offset));
            offset += textureResource.size + textureResource.size % newRequirements.alignment;
        }
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

    texture resource_pool::get_texture(u32 id) const
    {
        auto& resource = m_textureResources[id];

        return {
            .image = resource.image,
            .initializer = resource.initializer,
        };
    }

    void resource_pool::free_last_frame_resources(vulkan_context& ctx)
    {
        const auto submitIndex = ctx.get_submit_index() - 1;

        for (const auto& resource : m_lastFrameTextureResources)
        {
            ctx.destroy_deferred(resource.image, submitIndex);
        }

        if (m_lastFrameAllocation)
        {
            ctx.destroy_deferred(m_lastFrameAllocation, submitIndex);
        }
    }
}