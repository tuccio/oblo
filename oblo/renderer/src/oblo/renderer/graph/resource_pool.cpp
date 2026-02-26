#include <oblo/renderer/graph/resource_pool.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/reflection/struct_compare.hpp>
#include <oblo/core/reflection/struct_hash.hpp>
#include <oblo/core/string/debug_label.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/utility/convert_enum.hpp>
#include <oblo/gpu/vulkan/utility/image_utils.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/renderer/draw/monotonic_gbu_buffer.hpp>
#include <oblo/renderer/graph/frame_graph_resources_impl.hpp>

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

namespace oblo
{
    namespace
    {
        constexpr u32 FramesBeforeDeletingStableResources{1};
    }

    struct resource_pool::texture_resource
    {
        h32<gpu::image> handle{};
        gpu::image_descriptor descriptor;
        lifetime_range range;
        h32<stable_texture_resource> stableId;
        u32 framesAlive;
        bool isExternal;
    };

    struct resource_pool::buffer_resource
    {
        u64 offset;
        u64 size;
        flags<gpu::buffer_usage> usage;
        h32<gpu::buffer> buffer;
        h32<stable_buffer_resource> stableId;
        u32 framesAlive;
    };

    struct resource_pool::buffer_pool
    {
        monotonic_gpu_buffer buffer;
        flags<gpu::buffer_usage> usage;
    };

    struct resource_pool::stable_texture
    {
        h32<gpu::image> allocatedImage{};
        u32 creationTime;
        u32 lastUsedTime;
    };

    struct resource_pool::stable_buffer
    {
        h32<gpu::buffer> allocatedBuffer{};
        u32 creationTime;
        u32 lastUsedTime;

        flags<gpu::pipeline_sync_stage> previousStages{};
        flags<gpu::memory_access_type> previousAccess{};

        buffer_access_kind previousAccessKind{};
    };

    resource_pool::resource_pool() = default;

    resource_pool::~resource_pool() = default;

    bool resource_pool::init(gpu::gpu_instance& gpu)
    {
        auto* const vk = dynamic_cast<gpu::gpu_instance*>(&gpu);

        if (!vk)
        {
            return false;
        }

        constexpr u32 bufferChunkSize{1u << 20};

        struct buffer_pool_desc
        {
            flags<gpu::buffer_usage> flags;
            u8 alignment;
        };

        const gpu::device_info info = gpu.get_device_info();

        const buffer_pool_desc descs[] = {
            {
                .flags = gpu::buffer_usage::transfer_destination | gpu::buffer_usage::uniform,
                .alignment = narrow_cast<u8>(info.minUniformBufferOffsetAlignment),
            },
            {
                .flags = gpu::buffer_usage::transfer_source | gpu::buffer_usage::transfer_destination |
                    gpu::buffer_usage::index | gpu::buffer_usage::storage | gpu::buffer_usage::device_address,
                .alignment = narrow_cast<u8>(info.minStorageBufferOffsetAlignment),
            },
            {
                .flags = gpu::buffer_usage::transfer_source | gpu::buffer_usage::transfer_destination |
                    gpu::buffer_usage::storage | gpu::buffer_usage::indirect,
                .alignment =
                    narrow_cast<u8>(info.minStorageBufferOffsetAlignment), // TODO: Is there a min indirect alignment?
            },
        };

        m_bufferPools.reserve(array_size(descs));

        for (const auto& desc : descs)
        {
            auto& pool = m_bufferPools.emplace_back();
            pool.usage = desc.flags;
            pool.buffer.init(desc.flags, gpu::memory_usage::gpu_only, desc.alignment, bufferChunkSize);
        }

        return true;
    }

    void resource_pool::shutdown(gpu::gpu_instance& ctx)
    {
        m_lastFramePool = m_currentFramePool;
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

        m_textureResources.clear();

        m_lastFramePool = m_currentFramePool;
        m_currentFramePool = {};

        for (auto& pool : m_bufferPools)
        {
            pool.buffer.restore_all();
        }

        m_bufferResources.clear();
    }

    void resource_pool::end_build(gpu::gpu_instance& gpu)
    {
        // TODO: Here we should check if we can reuse the allocation from last frame, instead for now we
        // simply free the objects from last frame
        free_last_frame_resources(gpu);

        create_textures(gpu);
        create_buffers(gpu);

        free_stable_textures(gpu, FramesBeforeDeletingStableResources);
        free_stable_buffers(gpu, FramesBeforeDeletingStableResources);
    }

    h32<transient_texture_resource> resource_pool::add_transient_texture(
        const gpu::image_descriptor& descriptor, lifetime_range range, h32<stable_texture_resource> stableId)
    {
        const auto id = u32(m_textureResources.size());

        m_textureResources.push_back({
            .descriptor = descriptor,
            .range = range,
            .stableId = stableId,
        });

        return h32<transient_texture_resource>{id + 1};
    }

    h32<transient_texture_resource> resource_pool::add_texture_impl(const frame_graph_texture_impl& t, bool isExternal)
    {
        const auto id = m_textureResources.size32();

        m_textureResources.push_back({
            .handle = t.handle,
            .descriptor = t.descriptor,
            .isExternal = isExternal,
        });

        return h32<transient_texture_resource>{id + 1};
    }

    h32<transient_texture_resource> resource_pool::add_external_texture(gpu::gpu_instance& gpu,
        h32<gpu::image> externalImage)
    {
        const frame_graph_texture_impl t{
            .handle = externalImage,
            .descriptor = gpu.get_image_descriptor(externalImage),
        };

        return add_texture_impl(t, true);
    }

    h32<transient_texture_resource> resource_pool::add_external_texture(const frame_graph_texture_impl& t)
    {
        return add_texture_impl(t, true);
    }

    h32<transient_buffer_resource> resource_pool::add_transient_buffer(
        u64 size, flags<gpu::buffer_usage> usage, h32<stable_buffer_resource> stableId)
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
        gpu::image_usage usage)
    {
        OBLO_ASSERT(transientTexture);
        m_textureResources[transientTexture.value - 1].descriptor.usages |= usage;
    }

    void resource_pool::add_transient_buffer_usage(h32<transient_buffer_resource> transientBuffer,
        gpu::buffer_usage usage)
    {
        OBLO_ASSERT(transientBuffer);
        m_bufferResources[transientBuffer.value - 1].usage |= usage;
    }

    frame_graph_texture_impl resource_pool::get_transient_texture(h32<transient_texture_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_textureResources[id.value - 1];

        return {
            .handle = resource.handle,
            .descriptor = resource.descriptor,
        };
    }

    frame_graph_buffer_impl resource_pool::get_transient_buffer(h32<transient_buffer_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_bufferResources[id.value - 1];

        return {
            .handle = resource.buffer,
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

    const gpu::image_descriptor& resource_pool::get_initializer(h32<transient_texture_resource> id) const
    {
        OBLO_ASSERT(id);
        auto& resource = m_textureResources[id.value - 1];
        return resource.descriptor;
    }

    void resource_pool::fetch_buffer_tracking(h32<transient_buffer_resource> id,
        flags<gpu::pipeline_sync_stage>* stages,
        flags<gpu::memory_access_type>* access,
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
        flags<gpu::pipeline_sync_stage> stages,
        flags<gpu::memory_access_type> access,
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

    void resource_pool::free_last_frame_resources(gpu::gpu_instance& ctx)
    {
        if (m_lastFramePool)
        {
            const auto submitIndex = ctx.get_submit_index() - 1;

            ctx.destroy_deferred(m_lastFramePool, submitIndex);
            m_lastFramePool = {};
        }
    }

    void resource_pool::create_textures(gpu::gpu_instance& gpu)
    {
        dynamic_array<gpu::image_descriptor> descriptors;
        descriptors.reserve(m_textureResources.size());

        // For now we just allocate all textures
        for (auto& textureResource : m_textureResources)
        {
            if (textureResource.stableId)
            {
                acquire_from_pool(gpu, textureResource);
                continue;
            }

            if (textureResource.isExternal)
            {
                continue;
            }

            descriptors.emplace_back(textureResource.descriptor);
        }

        dynamic_array<h32<gpu::image>> images;
        descriptors.resize_default(descriptors.size());

        const expected pool = gpu.create_image_pool(descriptors, images);

        if (!pool)
        {
            return;
        }

        for (usize i = 0; i < m_textureResources.size(); ++i)
        {
            auto& textureResource = m_textureResources[i];

            if (textureResource.stableId || textureResource.isExternal)
            {
                continue;
            }

            textureResource.handle = images[i];
        }
    }

    void resource_pool::create_buffers(gpu::gpu_instance& ctx)
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
                if (pool.usage.contains_all(buffer.usage))
                {
                    poolBuffer = &pool.buffer;
                    break;
                }
            }

            OBLO_ASSERT(poolBuffer, "Couldn't find compatible pool");

            const auto r = poolBuffer->allocate(ctx, buffer.size);

            if (r)
            {
                buffer.buffer = r->buffer;
                buffer.offset = r->offset;
                buffer.size = r->size;
            }
        }
    }

    bool resource_pool::stable_texture_key::operator==(const stable_texture_key& rhs) const
    {
        return stableId == rhs.stableId && struct_compare<equal_to>(descriptor, rhs.descriptor);
    }

    usize resource_pool::stable_texture_key_hash::operator()(const stable_texture_key& key) const
    {
        return struct_hash<std::hash>(key);
    }

    usize resource_pool::stable_buffer_key_hash::operator()(const stable_buffer_key& key) const
    {
        return struct_hash<std::hash>(key);
    }

    void resource_pool::acquire_from_pool(gpu::gpu_instance& ctx, texture_resource& resource)
    {
        OBLO_ASSERT(resource.stableId);

        const auto [it, isNew] =
            m_stableTextures.try_emplace(stable_texture_key{resource.stableId, resource.descriptor});

        if (isNew)
        {
            const expected newImage = ctx.create_image(resource.descriptor);
            newImage.assert_value();

            it->second.allocatedImage = *newImage;
            it->second.creationTime = m_frame;
        }

        resource.framesAlive = m_frame - it->second.creationTime;

        it->second.lastUsedTime = m_frame;
    }

    void resource_pool::acquire_from_pool(gpu::gpu_instance& ctx, buffer_resource& resource)
    {
        OBLO_ASSERT(resource.stableId);
        const auto [it, isNew] = m_stableBuffers.try_emplace(stable_buffer_key{
            .stableId = resource.stableId,
            .usage = resource.usage,
            .size = resource.size,
        });

        if (isNew)
        {
            const expected newBuffer = ctx.create_buffer({
                .size = resource.size,
                .memoryProperties = gpu::memory_usage::gpu_only,
                .usages = resource.usage,
            });

            it->second.allocatedBuffer = *newBuffer;
            it->second.creationTime = m_frame;
        }

        resource.buffer = it->second.allocatedBuffer;
        resource.offset = 0;
        resource.framesAlive = m_frame - it->second.creationTime;

        it->second.lastUsedTime = m_frame;
    }

    void resource_pool::free_stable_textures(gpu::gpu_instance& ctx, u32 unusedFor)
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
                ctx.destroy_deferred(it->second.allocatedImage, submitIndex);

                it = m_stableTextures.erase(it);
            }
        }
    }

    void resource_pool::free_stable_buffers(gpu::gpu_instance& ctx, u32 unusedFor)
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
                ctx.destroy_deferred(it->second.allocatedBuffer, submitIndex);
                it = m_stableBuffers.erase(it);
            }
        }
    }
}