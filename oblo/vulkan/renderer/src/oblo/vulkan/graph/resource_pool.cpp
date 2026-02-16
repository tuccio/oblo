#include <oblo/vulkan/graph/resource_pool.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/reflection/struct_compare.hpp>
#include <oblo/core/reflection/struct_hash.hpp>
#include <oblo/gpu/vulkan/utility/image_utils.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/error.hpp>
#include <oblo/vulkan/monotonic_gbu_buffer.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

// For the sake of pooling textures, we ignore the debug labels
template <>
struct oblo::equal_to<oblo::debug_label>
{
    bool operator()(const oblo::debug_label&, const oblo::debug_label&) const
    {
        return true;
    }
};

template <>
struct std::hash<oblo::debug_label>
{
    size_t operator()(const oblo::debug_label&) const
    {
        return 0;
    }
};

namespace oblo::vk
{
    namespace
    {
        constexpr u32 FramesBeforeDeletingStableResources{1};
    }

    struct resource_pool::texture_resource
    {
        image_initializer initializer;
        lifetime_range range;
        VkImage image;
        VkImageView imageView;
        VkDeviceSize size;
        h32<stable_texture_resource> stableId;
        u32 framesAlive;
        bool isExternal;
    };

    struct resource_pool::buffer_resource
    {
        u32 size;
        VkBufferUsageFlags usage;
        VkBuffer buffer;
        u32 offset;
        h32<stable_buffer_resource> stableId;
        u32 framesAlive;
    };

    struct resource_pool::buffer_pool
    {
        monotonic_gpu_buffer buffer;
        VkBufferUsageFlags usage;
    };

    struct resource_pool::stable_texture
    {
        allocated_image allocatedImage;
        VkImageView imageView;
        u32 creationTime;
        u32 lastUsedTime;
    };

    struct resource_pool::stable_buffer
    {
        allocated_buffer allocatedBuffer;
        u32 creationTime;
        u32 lastUsedTime;

        VkPipelineStageFlags2 previousStages = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 previousAccess = VK_ACCESS_2_NONE;

        buffer_access_kind previousAccessKind{};
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
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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
        std::swap(m_lastFrameTransientTextures, m_textureResources);
        free_last_frame_resources(ctx);
        free_stable_textures(ctx, 0);
        free_stable_buffers(ctx, 0);

        for (auto& pool : m_bufferPools)
        {
            pool.buffer.shutdown(ctx);
        }
    }

    void resource_pool::begin_build()
    {
        ++m_frame;

        std::swap(m_lastFrameTransientTextures, m_textureResources);
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

        free_stable_textures(ctx, FramesBeforeDeletingStableResources);
        free_stable_buffers(ctx, FramesBeforeDeletingStableResources);
    }

    h32<transient_texture_resource> resource_pool::add_transient_texture(
        const image_initializer& initializer, lifetime_range range, h32<stable_texture_resource> stableId)
    {
        const auto id = u32(m_textureResources.size());

        m_textureResources.push_back({
            .initializer = initializer,
            .range = range,
            .stableId = stableId,
        });

        return h32<transient_texture_resource>{id + 1};
    }

    h32<transient_texture_resource> resource_pool::add_external_texture(const texture& texture)
    {
        const auto id = u32(m_textureResources.size());

        m_textureResources.push_back({
            .initializer = texture.initializer,
            .image = texture.image,
            .imageView = texture.view,
            .isExternal = true,
        });

        return h32<transient_texture_resource>{id + 1};
    }

    h32<transient_buffer_resource> resource_pool::add_transient_buffer(
        u32 size, VkBufferUsageFlags usage, h32<stable_buffer_resource> stableId)
    {
        const auto id = u32(m_bufferResources.size());

        m_bufferResources.push_back({
            .size = size,
            .usage = usage,
            .stableId = stableId,
        });

        return h32<transient_buffer_resource>{id + 1};
    }

    void resource_pool::add_transient_texture_usage(h32<transient_texture_resource> transientTexture,
        VkImageUsageFlags usage)
    {
        OBLO_ASSERT(transientTexture);
        m_textureResources[transientTexture.value - 1].initializer.usage |= usage;
    }

    void resource_pool::add_transient_buffer_usage(h32<transient_buffer_resource> transientBuffer,
        VkBufferUsageFlags usage)
    {
        OBLO_ASSERT(transientBuffer);
        m_bufferResources[transientBuffer.value - 1].usage |= usage;
    }

    texture resource_pool::get_transient_texture(h32<transient_texture_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_textureResources[id.value - 1];

        return {
            .image = resource.image,
            .view = resource.imageView,
            .initializer = resource.initializer,
        };
    }

    buffer resource_pool::get_transient_buffer(h32<transient_buffer_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_bufferResources[id.value - 1];

        return {
            .buffer = resource.buffer,
            .offset = resource.offset,
            .size = resource.size,
        };
    }

    bool resource_pool::is_stable(h32<transient_buffer_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_bufferResources[id.value - 1];
        return bool{resource.stableId};
    }

    u32 resource_pool::get_frames_alive_count(h32<transient_texture_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_textureResources[id.value - 1];
        return resource.framesAlive;
    }

    u32 resource_pool::get_frames_alive_count(h32<transient_buffer_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_bufferResources[id.value - 1];
        return resource.framesAlive;
    }

    const image_initializer& resource_pool::get_initializer(h32<transient_texture_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_textureResources[id.value - 1];
        return resource.initializer;
    }

    void resource_pool::fetch_buffer_tracking(h32<transient_buffer_resource> id,
        VkPipelineStageFlags2* stages,
        VkAccessFlags2* access,
        buffer_access_kind* accessKind) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_bufferResources[id.value - 1];
        OBLO_ASSERT(resource.stableId);

        const auto& stableBuffer = m_stableBuffers.at(stable_buffer_key{
            .stableId = resource.stableId,
            .usage = resource.usage,
            .size = resource.size,
        });

        *stages = stableBuffer.previousStages;
        *access = stableBuffer.previousAccess;
        *accessKind = stableBuffer.previousAccessKind;
    }

    void resource_pool::store_buffer_tracking(h32<transient_buffer_resource> id,
        VkPipelineStageFlags2 stages,
        VkAccessFlags2 access,
        buffer_access_kind accessKind)
    {
        OBLO_ASSERT(id);
        auto& resource = m_bufferResources[id.value - 1];
        OBLO_ASSERT(resource.stableId);

        auto& stableBuffer = m_stableBuffers.at(stable_buffer_key{
            .stableId = resource.stableId,
            .usage = resource.usage,
            .size = resource.size,
        });

        stableBuffer.previousStages = stages;
        stableBuffer.previousAccess = access;
        stableBuffer.previousAccessKind = accessKind;
    }

    void resource_pool::free_last_frame_resources(vulkan_context& ctx)
    {
        const auto submitIndex = ctx.get_submit_index() - 1;

        for (const auto& resource : m_lastFrameTransientTextures)
        {
            if (resource.stableId || resource.isExternal)
            {
                continue;
            }

            ctx.destroy_deferred(resource.image, submitIndex);
            ctx.destroy_deferred(resource.imageView, submitIndex);
        }

        if (m_lastFrameAllocation)
        {
            ctx.destroy_deferred(m_lastFrameAllocation, submitIndex);
            m_lastFrameAllocation = {};
        }

        m_lastFrameTransientTextures.clear();
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
            if (textureResource.stableId)
            {
                acquire_from_pool(ctx, textureResource);
                continue;
            }

            if (textureResource.isExternal)
            {
                continue;
            }

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

        const auto debugUtils = ctx.get_debug_utils_object();

        VkDeviceSize offset{0};
        for (auto& textureResource : m_textureResources)
        {
            if (textureResource.stableId || textureResource.isExternal)
            {
                continue;
            }

            OBLO_VK_PANIC(allocator.bind_image_memory(textureResource.image, m_allocation, offset));
            offset += textureResource.size + textureResource.size % newRequirements.alignment;

            const expected imageView = gpu::vk::image_utils::create_image_view_2d(device,
                textureResource.image,
                textureResource.initializer.format,
                allocationCbs);

            textureResource.imageView = imageView.assert_value_or(nullptr);

            if (!textureResource.initializer.debugLabel.empty())
            {
                debugUtils.set_object_name(device,
                    textureResource.imageView,
                    textureResource.initializer.debugLabel.get());
            }
        }
    }

    void resource_pool::create_buffers(vulkan_context& ctx)
    {
        for (auto& buffer : m_bufferResources)
        {
            if (buffer.stableId)
            {
                acquire_from_pool(ctx, buffer);
                continue;
            }

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
            buffer.size = r.size;
        }
    }

    bool resource_pool::stable_texture_key::operator==(const stable_texture_key& rhs) const
    {
        return stableId == rhs.stableId && struct_compare<equal_to>(initializer, rhs.initializer);
    }

    usize resource_pool::stable_texture_key_hash::operator()(const stable_texture_key& key) const
    {
        return struct_hash<std::hash>(key);
    }

    usize resource_pool::stable_buffer_key_hash::operator()(const stable_buffer_key& key) const
    {
        return struct_hash<std::hash>(key);
    }

    void resource_pool::acquire_from_pool(vulkan_context& ctx, texture_resource& resource)
    {
        OBLO_ASSERT(resource.stableId);
        const auto [it, isNew] =
            m_stableTextures.try_emplace(stable_texture_key{resource.stableId, resource.initializer});

        if (isNew)
        {
            auto& allocator = ctx.get_allocator();
            OBLO_VK_PANIC(allocator.create_image(resource.initializer, &it->second.allocatedImage));

            auto device = ctx.get_device();

            const expected imageView = gpu::vk::image_utils::create_image_view_2d(device,
                it->second.allocatedImage.image,
                resource.initializer.format,
                allocator.get_allocation_callbacks());

            it->second.imageView = imageView.assert_value_or(nullptr);
            it->second.creationTime = m_frame;

            if (!resource.initializer.debugLabel.empty() && imageView)
            {
                ctx.get_debug_utils_object().set_object_name(device, *imageView, resource.initializer.debugLabel.get());
            }
        }

        resource.image = it->second.allocatedImage.image;
        resource.imageView = it->second.imageView;
        resource.framesAlive = m_frame - it->second.creationTime;

        it->second.lastUsedTime = m_frame;
    }

    void resource_pool::acquire_from_pool(vulkan_context& ctx, buffer_resource& resource)
    {
        OBLO_ASSERT(resource.stableId);
        const auto [it, isNew] = m_stableBuffers.try_emplace(stable_buffer_key{
            .stableId = resource.stableId,
            .usage = resource.usage,
            .size = resource.size,
        });

        if (isNew)
        {
            auto& allocator = ctx.get_allocator();

            OBLO_VK_PANIC(allocator.create_buffer(
                {
                    .size = resource.size,
                    .usage = resource.usage,
                    .memoryUsage = memory_usage::gpu_only,
                },
                &it->second.allocatedBuffer));

            it->second.creationTime = m_frame;
        }

        resource.buffer = it->second.allocatedBuffer.buffer;
        resource.offset = 0;
        resource.framesAlive = m_frame - it->second.creationTime;

        it->second.lastUsedTime = m_frame;
    }

    void resource_pool::free_stable_textures(vulkan_context& ctx, u32 unusedFor)
    {
        for (auto it = m_stableTextures.begin(); it != m_stableTextures.end();)
        {
            const auto dt = m_frame - it->second.lastUsedTime;

            if (dt < unusedFor)
            {
                ++it;
            }
            else
            {
                const auto submitIndex = ctx.get_submit_index();

                ctx.destroy_deferred(it->second.imageView, submitIndex);
                ctx.destroy_deferred(it->second.allocatedImage.image, submitIndex);
                ctx.destroy_deferred(it->second.allocatedImage.allocation, submitIndex);

                it = m_stableTextures.erase(it);
            }
        }
    }

    void resource_pool::free_stable_buffers(vulkan_context& ctx, u32 unusedFor)
    {
        for (auto it = m_stableBuffers.begin(); it != m_stableBuffers.end();)
        {
            const auto dt = m_frame - it->second.lastUsedTime;

            if (dt < unusedFor)
            {
                ++it;
            }
            else
            {
                const auto submitIndex = ctx.get_submit_index();
                ctx.destroy_deferred(it->second.allocatedBuffer.buffer, submitIndex);
                ctx.destroy_deferred(it->second.allocatedBuffer.allocation, submitIndex);
                it = m_stableBuffers.erase(it);
            }
        }
    }
}