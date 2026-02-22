#include <oblo/renderer/draw/texture_registry.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/gpu/gpu_queue_context.hpp>
#include <oblo/gpu/staging_buffer.hpp>
#include <oblo/gpu/structs.hpp>
#include <oblo/gpu/types.hpp>
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

    struct resident_texture
    {
        // Only set if the image is owned.
        h32<gpu::image> handle;

        VkImage image;
        VkImageView imageView;
    };

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

    bool texture_registry::init(
        gpu::vk::vulkan_instance& vkCtx, gpu::gpu_queue_context& queueCtx, gpu::staging_buffer& staging)
    {
        const u32 maxDescriptorCount = get_max_descriptor_count();

        m_imageInfo.reserve(maxDescriptorCount);
        m_textures.reserve(maxDescriptorCount);

        m_vkCtx = &vkCtx;
        m_queueCtx = &queueCtx;
        m_staging = &staging;

        return true;
    }

    void texture_registry::shutdown()
    {
        const auto submitIndex = m_queueCtx->get_submit_index();

        for (const auto& t : m_textures)
        {
            if (!t.handle)
            {
                // The dummy will be present multiple times here, but only one occurrence is owning the allocation
                continue;
            }

            m_queueCtx->destroy_deferred(t.handle, submitIndex);
        }
    }

    void texture_registry::on_first_frame()
    {
        texture_resource dummy;

        // TODO: Make it a more recognizable texture, since sampling it should only happen by mistake
        resident_texture residentTexture;

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
            !create(dummy, residentTexture, {"dummy_fallback_texture"}))
        {
            log::error("Failed to allocate fallback taxture");
            return;
        }

        const VkDescriptorImageInfo dummyInfo{
            .imageView = residentTexture.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        m_imageInfo.assign(1, dummyInfo);
        m_textures.assign(1, residentTexture);
    }

    h32<resident_texture> texture_registry::acquire()
    {
        h32<resident_texture> res{};
        res = h32<resident_texture>{m_handlePool.acquire()};
        OBLO_ASSERT(res.value != 0);

        // Initialize to dummy texture to make it easier to debug
        resident_texture texture = m_textures[0];

        // We give out a non-owning reference, to make sure it's not double-deleted
        texture.handle = {};

        set_texture(res, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        return res;
    }

    bool texture_registry::set_texture(
        h32<resident_texture> h, const texture_resource& texture, const debug_label& debugName)
    {
        resident_texture residentTexture;

        if (!create(texture, residentTexture, debugName))
        {
            return false;
        }

        set_texture(h, residentTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return true;
    }

    void texture_registry::set_texture(h32<resident_texture> h, VkImageView view, VkImageLayout layout)
    {
        const resident_texture residentTexture{
            .imageView = view,
        };

        set_texture(h, residentTexture, layout);
    }

    void texture_registry::set_texture(
        h32<resident_texture> h, const resident_texture& residentTexture, VkImageLayout layout)
    {
        OBLO_ASSERT(h.value != 0);

        const auto index = m_handlePool.get_index(h.value);

        if (index >= m_imageInfo.size())
        {
            const auto newSize = index + 1;

            m_imageInfo.reserve_exponential(newSize);
            m_textures.reserve_exponential(newSize);

            m_imageInfo.resize(newSize);
            m_textures.resize(newSize);
        }

        OBLO_ASSERT(m_imageInfo[index].imageView == m_imageInfo[0].imageView || m_imageInfo[index].imageView == nullptr,
            "Replacing a different texture here would require destroying it");

        m_imageInfo[index] = {
            .imageView = residentTexture.imageView,
            .imageLayout = layout,
        };

        m_textures[index] = residentTexture;
    }

    h32<resident_texture> texture_registry::add(const texture_resource& texture, const debug_label& debugName)
    {
        resident_texture residentTexture;

        if (!create(texture, residentTexture, debugName))
        {
            return {};
        }

        const auto h = acquire();
        set_texture(h, residentTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return h;
    }

    void texture_registry::remove(h32<resident_texture> texture)
    {
        // Destroy the texture, fill the hole with the dummy

        auto index = m_handlePool.get_index(texture.value);
        OBLO_ASSERT(index != 0 && index < m_imageInfo.size());

        auto& t = m_textures[index];

        if (t.handle)
        {
            const auto submitIndex = m_queueCtx->get_submit_index();
            m_queueCtx->destroy_deferred(t.handle, submitIndex);
        }

        // Reset to the dummy
        t = m_textures[0];
        t.handle = {};

        m_imageInfo[index] = m_imageInfo[0];

        m_handlePool.release(texture.value);
    }

    std::span<const VkDescriptorImageInfo> texture_registry::get_textures2d_info() const
    {
        return m_imageInfo;
    }

    u32 texture_registry::get_max_descriptor_count() const
    {
        return 2048;
    }

    void texture_registry::flush_uploads(hptr<gpu::command_buffer> commandBuffer)
    {
        buffered_array<gpu::buffer_image_copy_descriptor, 16> copies;

        constexpr auto initialImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        constexpr auto finalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        constexpr auto aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

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
                            .aspectMask = gpu::image_aspect::color,
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

            const VkImageSubresourceRange pipelineRange{
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = upload.levels.size32(),
                .baseArrayLayer = 0,
                .layerCount = 1,
            };

            const VkImage vkImage = m_vkCtx->unwrap_image(upload.handle);
            const VkCommandBuffer vkCmdBuffer = m_vkCtx->unwrap_command_buffer(commandBuffer);

            gpu::vk::add_pipeline_barrier_cmd(vkCmdBuffer,
                initialImageLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                vkImage,
                VkFormat(upload.format),
                pipelineRange);

            m_staging->upload(commandBuffer, upload.handle, copies);

            gpu::vk::add_pipeline_barrier_cmd(vkCmdBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                finalImageLayout,
                vkImage,
                VkFormat(upload.format),
                pipelineRange);
        }

        m_pendingUploads.clear();
    }

    bool texture_registry::create(const texture_resource& texture, resident_texture& out, const debug_label& debugName)
    {
        out = resident_texture{};

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
            .type = *imageType,
            .samples = gpu::samples_count::one,
            .usages = gpu::image_usage::shader_sample | gpu::image_usage::transfer_destination,
            .debugLabel = debugName,
        };

        const expected image = m_vkCtx->create_image(descriptor);

        if (!image)
        {
            return false;
        }

        auto& textureUpload = m_pendingUploads.push_back({
            .handle = out.handle,
            .format = srcFormat,
            .width = desc.width,
            .height = desc.height,
        });

        textureUpload.levels.reserve(desc.numLevels);

        const auto cleanupAfterFailure = [this, &image]
        {
            m_pendingUploads.pop_back();
            m_vkCtx->destroy_image(*image);
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