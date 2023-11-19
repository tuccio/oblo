#include <oblo/vulkan/draw/texture_registry.hpp>

#include <oblo/scene/assets/texture.hpp>
#include <oblo/vulkan/staging_buffer.hpp>
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

    bool texture_registry::init(vulkan_context& vkCtx, staging_buffer& staging)
    {
        m_imageInfo.reserve(2048);
        m_textures.reserve(2048);

        m_vkCtx = &vkCtx;

        texture_resource dummy;

        // TODO: Make it a more recognizable texture, since sampling it should only happen by mistake
        dummy.allocate({
            .vkFormat = VK_FORMAT_R8G8B8_SNORM,
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

        if (!create(staging, dummy, residentTexture))
        {
            return false;
        }

        const VkDescriptorImageInfo dummyInfo{
            .imageView = residentTexture.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        m_imageInfo.assign(1, dummyInfo);
        m_textures.assign(1, residentTexture);

        return true;
    }

    h32<resident_texture> texture_registry::add(staging_buffer& staging, const texture_resource& texture)
    {
        resident_texture residentTexture;

        if (!create(staging, texture, residentTexture))
        {
            return {};
        }

        h32<resident_texture> res{};
        res = h32<resident_texture>{m_handlePool.acquire()};
        OBLO_ASSERT(res.value != 0);

        const auto index = m_handlePool.get_index(res.value);

        if (index >= m_imageInfo.size())
        {
            const auto newCapacity = index * 2;
            const auto newSize = index + 1;

            m_imageInfo.reserve(newCapacity);
            m_imageInfo.resize(newSize);

            m_textures.reserve(newCapacity);
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

    bool texture_registry::create(staging_buffer& staging, const texture_resource& texture, resident_texture& out)
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

        const auto format = VkFormat(desc.vkFormat);

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
                    .levelCount = 1,
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

        // TODO: Upload texture
        // staging.upload();
        (void) staging;

        return true;
    }
}