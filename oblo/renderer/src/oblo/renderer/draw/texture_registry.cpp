#include <oblo/renderer/draw/texture_registry.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/span.hpp>
#include <oblo/gpu/enums.hpp>
#include <oblo/gpu/gpu_instance.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/vulkan/utility/pipeline_barrier.hpp>
#include <oblo/gpu/vulkan/vulkan_instance.hpp>
#include <oblo/log/log.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/scene/resources/texture_format.hpp>

namespace oblo
{
    namespace
    {
        expected<gpu::image_type> deduce_image_type(const texture_desc& desc)
        {
            switch (desc.dimensions)
            {
            case 1:
                return gpu::image_type::plain_1d;

            case 2:
                return gpu::image_type::plain_2d;

            case 3:
                return gpu::image_type::plain_3d;

            default:
                return "Unhandled image type"_err;
            }
        }

        struct upload_level
        {
            using segment = std::decay_t<decltype(gpu::staging_buffer_span{}.segments[0])>;
            segment data;
        };
    }

    struct texture_registry::pending_texture_upload
    {
        h32<gpu::image> handle;
        gpu::image_format format;
        u32 width;
        u32 height;
        dynamic_array<upload_level> levels;
    };

    texture_registry::texture_registry() = default;

    texture_registry::~texture_registry() = default;

    bool texture_registry::init(gpu::gpu_instance& gpu, gpu::staging_buffer& staging)
    {
        const u32 maxDescriptorCount = gpu.get_max_bindless_images();

        // Zero-initialize to set isOwned to false on all slots
        m_textures.resize(maxDescriptorCount);
        m_isOwned.resize(maxDescriptorCount);
        m_usedSlots = 0;

        m_gpu = &gpu;
        m_staging = &staging;

        return true;
    }

    void texture_registry::shutdown()
    {
        const auto submitIndex = m_gpu->get_submit_index();

        for (u32 i = 0; i < m_usedSlots; ++i)
        {
            if (!m_isOwned[i])
            {
                // The dummy will be present multiple times here, but only one occurrence is owning the allocation
                continue;
            }

            m_gpu->destroy_deferred(m_textures[i].image, submitIndex);
        }
    }

    void texture_registry::on_first_frame()
    {
        texture_resource dummy;

        // TODO: Make it a more recognizable texture, since sampling it should only happen by mistake
        gpu::bindless_image_descriptor residentTexture;

        if (!dummy.allocate({
                .vkFormat = oblo::texture_format::r8g8b8a8_unorm,
                .width = 1,
                .height = 1,
                .depth = 1,
                .dimensions = 2,
                .numLevels = 1,
                .numLayers = 1,
                .numFaces = 1,
                .isArray = false,
            }) ||
            !create_texture(dummy, residentTexture, {"dummy_fallback_texture"}))
        {
            log::error("Failed to allocate fallback taxture");
            return;
        }

        set_texture_impl({}, residentTexture, true);
    }

    h32<resident_texture> texture_registry::acquire()
    {
        const h32<resident_texture> h{m_handlePool.acquire()};
        OBLO_ASSERT(h.value != 0);

        // Initialize to dummy texture to make it easier to debug
        // We give out a non-owning reference, to make sure it's not double-deleted
        const gpu::bindless_image_descriptor residentTexture = m_textures[0];

        set_texture_impl(h, residentTexture, false);

        return h;
    }

    bool texture_registry::set_texture(
        h32<resident_texture> h, const texture_resource& texture, const debug_label& debugName)
    {
        gpu::bindless_image_descriptor residentTexture;

        if (!create_texture(texture, residentTexture, debugName))
        {
            return false;
        }

        set_texture_impl(h, residentTexture, true);
        return true;
    }

    void texture_registry::set_external_texture(
        h32<resident_texture> h, h32<gpu::image> image, gpu::image_resource_state state)
    {
        OBLO_ASSERT(image);
        const gpu::bindless_image_descriptor residentTexture{
            .image = image,
            .state = state,
        };

        set_texture_impl(h, residentTexture, false);
    }

    void texture_registry::set_texture_impl(
        h32<resident_texture> h, const gpu::bindless_image_descriptor& residentTexture, bool isOwned)
    {
        const u32 index = m_handlePool.get_index(h.value);

        m_usedSlots = max(index + 1, m_usedSlots);

        OBLO_ASSERT(!m_isOwned[index], "Replacing an owned texture here would require destroying it");

        m_textures[index] = residentTexture;
        m_isOwned[index] = isOwned;
    }

    h32<resident_texture> texture_registry::add(const texture_resource& texture, const debug_label& debugName)
    {
        gpu::bindless_image_descriptor residentTexture;

        if (!create_texture(texture, residentTexture, debugName))
        {
            return {};
        }

        const auto h = acquire();
        set_texture_impl(h, residentTexture, true);
        return h;
    }

    void texture_registry::remove(h32<resident_texture> texture)
    {
        // Destroy the texture, fill the hole with the dummy

        auto index = m_handlePool.get_index(texture.value);
        OBLO_ASSERT(index != 0 && index < m_textures.size());

        gpu::bindless_image_descriptor& image = m_textures[index];
        bool& isOwned = m_isOwned[index];

        if (isOwned)
        {
            const auto submitIndex = m_gpu->get_submit_index();
            m_gpu->destroy_deferred(image.image, submitIndex);
        }

        // Reset to the dummy
        image = m_textures[0];
        isOwned = false;

        m_handlePool.release(texture.value);
    }

    void texture_registry::flush_uploads(hptr<gpu::command_buffer> commandBuffer)
    {
        buffered_array<gpu::buffer_image_copy_descriptor, 16> copies;

        for (const auto& upload : m_pendingUploads)
        {
            copies.clear();
            copies.reserve(upload.levels.size());

            for (u32 levelIndex = 0; levelIndex < upload.levels.size32(); ++levelIndex)
            {
                const auto& level = upload.levels[levelIndex];
                const auto& segment = level.data;

                copies.push_back(gpu::buffer_image_copy_descriptor{
                    .bufferOffset = segment.begin,
                    .imageSubresource =
                        {
                            .mipLevel = levelIndex,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                    .imageOffset = {},
                    .imageExtent =
                        {
                            upload.width >> levelIndex,
                            upload.height >> levelIndex,
                            1,
                        },
                });

                auto& lastCopy = copies.back();
                lastCopy.imageSubresource.mipLevel = levelIndex;
            }

            m_gpu->cmd_apply_barriers(commandBuffer,
                gpu::memory_barrier_descriptor{
                    .images = make_span_initializer<gpu::image_state_transition>({
                        {
                            .image = upload.handle,
                            .previousPipelines = gpu::pipeline_sync_stage::all_commands,
                            .previousState = gpu::image_resource_state::undefined,
                            .nextPipelines = gpu::pipeline_sync_stage::transfer,
                            .nextState = gpu::image_resource_state::transfer_destination,
                        },
                    }),
                });

            m_staging->upload(commandBuffer, upload.handle, copies);

            m_gpu->cmd_apply_barriers(commandBuffer,
                gpu::memory_barrier_descriptor{
                    .images = make_span_initializer<gpu::image_state_transition>({
                        {
                            .image = upload.handle,
                            .previousPipelines = gpu::pipeline_sync_stage::transfer,
                            .previousState = gpu::image_resource_state::transfer_destination,
                            .nextPipelines = gpu::pipeline_sync_stage::all_commands,
                            .nextState = gpu::image_resource_state::shader_read,
                        },
                    }),
                });
        }

        m_pendingUploads.clear();
    }

    void texture_registry::update_texture_bind_groups() const
    {
        if (m_usedSlots > 0)
        {
            m_gpu->set_bindless_images(std::span{m_textures}.subspan(0, m_usedSlots), 0).assert_value();
        }
    }

    u32 texture_registry::get_used_textures_slots() const
    {
        return m_usedSlots;
    }

    bool texture_registry::create_texture(
        const texture_resource& texture, gpu::bindless_image_descriptor& out, const debug_label& debugName)
    {
        out = {};

        const auto desc = texture.get_description();

        const auto imageType = deduce_image_type(desc);

        // We only support 2d textures for now
        if (!imageType || *imageType != gpu::image_type::plain_2d)
        {
            return false;
        }

        const gpu::image_format srcFormat = gpu::image_format(desc.vkFormat);

        const gpu::image_descriptor descriptor{
            .format = srcFormat,
            .width = desc.width,
            .height = desc.height,
            .depth = desc.depth,
            .mipLevels = desc.numLevels,
            .arrayLayers = 1,
            .type = *imageType,
            .samples = gpu::samples_count::one,
            .memoryUsage = gpu::memory_usage::gpu_only,
            .usages = gpu::image_usage::shader_sample | gpu::image_usage::transfer_destination,
            .debugLabel = debugName,
        };

        const expected image = m_gpu->create_image(descriptor);

        if (!image)
        {
            OBLO_ASSERT(image);
            return false;
        }

        // After upload it will be in shader read stage, so we set it right away
        out.state = gpu::image_resource_state::shader_read;
        out.image = *image;

        auto& textureUpload = m_pendingUploads.push_back({
            .handle = out.image,
            .format = srcFormat,
            .width = desc.width,
            .height = desc.height,
        });

        textureUpload.levels.reserve(desc.numLevels);

        const auto cleanupAfterFailure = [this, &image]
        {
            m_pendingUploads.pop_back();
            m_gpu->destroy(*image);
        };

        const u32 texelSize = texture.get_element_size();

        for (u32 i = 0; i < desc.numLevels; ++i)
        {
            const auto data = texture.get_data(i, 0, 0);
            const auto staged = m_staging->stage_image(data, texelSize);

            if (!staged)
            {
                cleanupAfterFailure();
                return false;
            }

            textureUpload.levels.push_back({
                .data = staged->segments[0],
            });
        }

        return true;
    }
}