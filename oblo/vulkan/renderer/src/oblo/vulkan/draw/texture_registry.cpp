#include <oblo/vulkan/draw/texture_registry.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/scene/resources/texture_format.hpp>
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

        struct upload_level
        {
            using segment = std::decay_t<decltype(staging_buffer_span{}.segments[0])>;
            segment data;
        };
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
        VkImage image;
        VkFormat format;
        u32 width;
        u32 height;
        dynamic_array<upload_level> levels;
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
            if (!t.image.allocation)
            {
                // The dummy will be present multiple times here, but only one occurrence is owning the allocation
                continue;
            }

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
            .vkFormat = texture_format::r8g8b8a8_unorm,
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

    h32<resident_texture> texture_registry::acquire()
    {
        h32<resident_texture> res{};
        res = h32<resident_texture>{m_handlePool.acquire()};
        OBLO_ASSERT(res.value != 0);

        // Initialize to dummy texture to make it easier to debug
        resident_texture texture = m_textures[0];

        // We give out a non-owning reference, to make sure it's not double-deleted
        texture.image.allocation = nullptr;

        set_texture(res, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        return res;
    }

    void texture_registry::set_texture(h32<resident_texture> h, VkImageView imageView, VkImageLayout layout)
    {
        set_texture(h, resident_texture{.imageView = imageView}, layout);
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

    void texture_registry::flush_uploads(VkCommandBuffer commandBuffer)
    {
        buffered_array<VkBufferImageCopy, 16> copies;

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

                copies.push_back(VkBufferImageCopy{
                    .bufferOffset = segment.begin,
                    .imageSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = levelIndex,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        },
                    .imageOffset = VkOffset3D{},
                    .imageExtent =
                        VkExtent3D{
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

            add_pipeline_barrier_cmd(commandBuffer,
                initialImageLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                upload.image,
                upload.format,
                pipelineRange);

            m_staging->upload(commandBuffer, upload.image, copies);

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
        bool success = false;

        out = resident_texture{};

        auto& allocator = m_vkCtx->get_allocator();

        const auto cleanup = finally(
            [&success, &out, &allocator]
            {
                if (success)
                {
                    return;
                }

                if (out.image.allocation)
                {
                    allocator.destroy(out.image);
                    out.image = {};
                }

                if (out.imageView)
                {
                    vkDestroyImageView(allocator.get_device(), out.imageView, allocator.get_allocation_callbacks());
                    out.imageView = {};
                }
            });

        const auto desc = texture.get_description();

        const auto imageType = deduce_image_type(desc);

        // We only support 2d textures for now
        if (imageType != VK_IMAGE_TYPE_2D)
        {
            return success;
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
            return success;
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
            return success;
        }

        out.imageView = newImageView;

        auto& textureUpload = m_pendingUploads.push_back({
            .image = out.image.image,
            .format = format,
            .width = desc.width,
            .height = desc.height,
        });

        textureUpload.levels.reserve(desc.numLevels);

        const u32 texelSize = texture.get_element_size();

        for (u32 i = 0; i < desc.numLevels; ++i)
        {
            const auto data = texture.get_data(i, 0, 0);
            const auto staged = m_staging->stage_image(data, texelSize);

            if (!staged)
            {
                m_pendingUploads.pop_back();
                return success;
            }

            textureUpload.levels.push_back({
                .data = staged->segments[0],
            });
        }

        // This is so that the cleanup functor doesn't do anything
        success = true;
        return success;
    }
}