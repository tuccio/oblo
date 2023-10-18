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

    void resource_pool::begin_build(u64 frameIndex)
    {
        // Keep track of frame index in case we need to free memory
        (void) frameIndex;

        // Destroy last frame images? Or queue for destruction into vulkan context?
        // Free the memory allocation?
        // Maybe delay destruction to later to figure if it's necessary (e.g. allow caching)
        m_graphBegin = 0;
        m_textureResources.clear();
    }

    void resource_pool::end_build(const vulkan_context& ctx)
    {
        VkDevice device{ctx.get_device()};

        auto& allocator = ctx.get_allocator();
        auto* const allocationCbs = allocator.get_allocation_callbacks();

        VkMemoryRequirements newRequirements{.memoryTypeBits = ~u32{}};

        // For new we jsut allocate all textures
        for (auto& textureResource : m_textureResources)
        {
            const auto& initializer = textureResource.initializer;

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

        OBLO_ASSERT(newRequirements.memoryTypeBits != 0);

        if (newRequirements.size == 0)
        {
            // Nothing to do?
            return;
        }

        // Add space for alignment
        newRequirements.size += (newRequirements.alignment - 1) * m_textureResources.size();

        // TODO: Store it, determine when to free it
        const auto allocation = allocator.create_memory(newRequirements);

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
}