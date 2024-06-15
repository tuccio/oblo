#include <oblo/vulkan/draw/texture_registry.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/scene/assets/texture.hpp>
#include <oblo/vulkan/staging_buffer.hpp>
#include <oblo/vulkan/utility/pipeline_barrier.hpp>
#include <oblo/vulkan/vulkan_context.hpp>

namespace oblo::vk
{
    namespace
    {
        VkImageType deduce_image_type(const texture_desc& desc)
        {
            switch (desc.dimensions)
            {
            case 1:
                return VK_IMAGE_TYPE_1D;

            case 2:
                return VK_IMAGE_TYPE_2D;

            case 3:
                return VK_IMAGE_TYPE_3D;

            default:
                return VK_IMAGE_TYPE_MAX_ENUM;
            }
        }
    }

    struct resident_texture
    {
        // The image is only needs to be set if the texture is owned,
        // we only care about the image view otherwise
        allocated_image image;
        VkImageView imageView;
    };

    struct texture_registry::pending_texture_upload
    {
        staging_buffer_span src;
        VkImage image;
        VkFormat format;
        u32 width;
        u32 height;
        u32 levels;
        dynamic_array<u32> levelOffsets;
    };

    texture_registry::texture_registry() = default;

    texture_registry::~texture_registry() = default;

    bool texture_registry::init(vulkan_context& vkCtx, staging_buffer& staging)
    {
        const u32 maxDescriptorCount = get_max_descriptor_count();

        m_imageInfo.reserve(maxDescriptorCount);
        m_textures.reserve(maxDescriptorCount);

        m_vkCtx = &vkCtx;
        m_staging = &staging;

        return true;
    }

    void texture_registry::shutdown()
    {
        const auto submitIndex = m_vkCtx->get_submit_index();

        for (const auto& t : m_textures)
        {
            m_vkCtx->destroy_deferred(t.image.image, submitIndex);
            m_vkCtx->destroy_deferred(t.image.allocation, submitIndex);
            m_vkCtx->destroy_deferred(t.imageView, submitIndex);
        }
    }

    void texture_registry::on_first_frame()
    {
        texture_resource dummy;

        // TODO: Make it a more recognizable texture, since sampling it should only happen by mistake
        dummy.allocate({
            .vkFormat = VK_FORMAT_R8G8B8A8_SRGB,
            .width = 1,
            .height = 1,
            .depth = 1,
            .dimensions = 2,
            .numLevels = 1,
            .numLayers = 1,
            .numFaces = 1,
            .isArray = false,
        });

        resident_texture residentTexture;

        if (!create(dummy, residentTexture, {"dummy_fallback_texture"}))
        {
            return;
        }

        const VkDescriptorImageInfo dummyInfo{
            .imageView = residentTexture.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        m_imageInfo.assign(1, dummyInfo);
        m_textures.assign(1, residentTexture);
    }

    h32<resident_texture> texture_registry::add(const texture_resource& texture, const debug_label& debugName)
    {
        resident_texture residentTexture;

        if (!create(texture, residentTexture, debugName))
        {
            return {};
        }

        h32<resident_texture> res{};
        res = h32<resident_texture>{m_handlePool.acquire()};
        OBLO_ASSERT(res.value != 0);

        const auto index = m_handlePool.get_index(res.value);

        if (index >= m_imageInfo.size())
        {
            const auto newSize = index + 1;

            m_imageInfo.resize(newSize);
            m_textures.resize(newSize);

            m_imageInfo[index] = {
                .imageView = residentTexture.imageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            m_textures[index] = residentTexture;
        }

        return res;
    }

    void texture_registry::remove(h32<resident_texture> texture)
    {
        // Destroy the texture, fill the hole with the dummy

        auto index = m_handlePool.get_index(texture.value);
        OBLO_ASSERT(index != 0 && index < m_imageInfo.size());

        auto& t = m_textures[index];

        if (t.image.allocation)
        {
            const auto submitIndex = m_vkCtx->get_submit_index();
            m_vkCtx->destroy_deferred(t.image.allocation, submitIndex);
            m_vkCtx->destroy_deferred(t.image.image, submitIndex);
            m_vkCtx->destroy_deferred(t.imageView, submitIndex);
        }

        // Reset to the dummy
        t = m_textures[0];
        t.image = {};

        m_imageInfo[index] = m_imageInfo[0];
    }

    std::span<const VkDescriptorImageInfo> texture_registry::get_textures2d_info() const
    {
        return m_imageInfo;
    }

    u32 texture_registry::get_max_descriptor_count() const
    {
        return 2048;
    }

    void texture_registry::flush_uploads(VkCommandBuffer commandBuffer)
    {
        buffered_array<VkBufferImageCopy, 14> copies;

        constexpr auto initialImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        constexpr auto finalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        constexpr auto aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        for (const auto& upload : m_pendingUploads)
        {
            const auto& segment = upload.src.segments[0];

            copies.clear();
            copies.reserve(upload.levels);

            for (u32 level = 0; level < upload.levels; ++level)
            {
                const auto bufferOffset = segment.begin + upload.levelOffsets[level];

                // Just detecting obvious mistakes, we should actually add the size of the upload
                OBLO_ASSERT(bufferOffset < segment.end);

                copies.push_back({
                    .bufferOffset = bufferOffset,
                    .imageSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = level,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                    .imageOffset = VkOffset3D{},
                    .imageExtent =
                        VkExtent3D{
                            upload.width >> level,
                            upload.height >> level,
                            1,
                        },
                });

                auto& lastCopy = copies.back();
                lastCopy.imageSubresource.mipLevel = level;
            }

            const VkImageSubresourceRange pipelineRange{
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = upload.levels,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };

            add_pipeline_barrier_cmd(commandBuffer,
                initialImageLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                upload.image,
                upload.format,
                pipelineRange);

            m_staging->upload(commandBuffer, upload.src, upload.image, copies);

            add_pipeline_barrier_cmd(commandBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                finalImageLayout,
                upload.image,
                upload.format,
                pipelineRange);
        }

        m_pendingUploads.clear();
    }

    bool texture_registry::create(const texture_resource& texture, resident_texture& out, const debug_label& debugName)
    {
        out = resident_texture{};

        const auto desc = texture.get_description();

        // TODO: Upload texture
        auto& allocator = m_vkCtx->get_allocator();

        const auto imageType = deduce_image_type(desc);

        // We only support 2d textures for now
        if (imageType != VK_IMAGE_TYPE_2D)
        {
            return false;
        }

        const auto srcFormat = VkFormat(desc.vkFormat);
        auto format = srcFormat;

        const image_initializer initializer{
            .imageType = imageType,
            .format = format,
            .extent = {desc.width, desc.height, desc.depth},
            .mipLevels = desc.numLevels,
            .arrayLayers = desc.numLayers,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .memoryUsage = memory_usage::gpu_only,
            .debugLabel = debugName,
        };

        if (allocator.create_image(initializer, &out.image) != VK_SUCCESS)
        {
            return false;
        }

        const VkImageViewCreateInfo imageViewInit{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = out.image.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = desc.numLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VkImageView newImageView;

        const auto imageViewRes = vkCreateImageView(allocator.get_device(),
            &imageViewInit,
            allocator.get_allocation_callbacks(),
            &newImageView);

        if (imageViewRes != VK_SUCCESS)
        {
            allocator.destroy(out.image);
            return false;
        }

        out.imageView = newImageView;

        // TOOD: When failing, we should destroy the texture

        const auto data = texture.get_data();
        const auto staged = m_staging->stage_image(data, format);

        OBLO_ASSERT(staged);

        if (!staged)
        {
            return false;
        }

        m_pendingUploads.push_back({
            .src = *staged,
            .image = out.image.image,
            .format = format,
            .width = desc.width,
            .height = desc.height,
            .levels = desc.numLevels,
        });

        auto& levelOffsets = m_pendingUploads.back().levelOffsets;
        levelOffsets.reserve(desc.numLevels);

        for (u32 level = 0; level < desc.numLevels; ++level)
        {
            levelOffsets.push_back(texture.get_offset(level, 0, 0));
        }

        return true;
    }
}