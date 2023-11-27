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

        bool convert_rgb8_to_rgba8(
            const texture_resource& source, const texture_desc& desc, texture_resource& converted, VkFormat newFormat)
        {
            auto convertedDesc = desc;
            convertedDesc.vkFormat = u32(newFormat);

            if (!converted.allocate(desc))
            {
                return false;
            }

            for (u32 level = 0; level < desc.numLevels; ++level)
            {
                for (u32 face = 0; face < desc.numFaces; ++face)
                {
                    for (u32 layer = 0; layer < desc.numLayers; ++layer)
                    {
                        const std::span src = source.get_data(level, face, layer);
                        const std::span dst = converted.get_data(level, face, layer);

                        auto* outIt = dst.data();
                        auto* inIt = src.data();

                        while (outIt != dst.data() + 4 * desc.width)
                        {
                            outIt[0] = inIt[0];
                            outIt[1] = inIt[1];
                            outIt[2] = inIt[2];
                            outIt[3] = std::byte(0xff);

                            outIt += 4;
                            inIt += 3;
                        }
                    }
                }
            }

            return true;
        }
    }

    struct resident_texture
    {
        // The image is only needs to be set if the texture is owned,
        // we only care about the image view otherwise
        allocated_image image;
        VkImageView imageView;
    };

    texture_registry::texture_registry() = default;

    texture_registry::~texture_registry() = default;

    bool texture_registry::init(vulkan_context& vkCtx, staging_buffer& staging)
    {
        const u32 maxDescriptorCount = get_max_descriptor_count();

        m_imageInfo.reserve(maxDescriptorCount);
        m_textures.reserve(maxDescriptorCount);

        m_vkCtx = &vkCtx;

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

    u32 texture_registry::get_max_descriptor_count() const
    {
        return 2048;
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

        const auto srcFormat = VkFormat(desc.vkFormat);
        auto format = srcFormat;

        bool convertRGB8toRGBA8{false};

        switch (srcFormat)
        {
        case VK_FORMAT_R8G8B8_SRGB:
            format = VK_FORMAT_R8G8B8A8_SNORM;
            convertRGB8toRGBA8 = true;
            break;

        default:
            break;
        }

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

        out.imageView = newImageView;

        const texture_resource* finalTexture{&texture};
        texture_resource converted;

        if (convertRGB8toRGBA8)
        {
            if (!convert_rgb8_to_rgba8(texture, desc, converted, format))
            {
                return false;
            }

            finalTexture = &converted;
        }

        // TOOD: When failing, we should destroy the texture
        // TODO: Mips

        return staging.upload(finalTexture->get_data(),
            out.image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            desc.width,
            desc.height,
            VkImageSubresourceLayers{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            },
            VkOffset3D{},
            VkExtent3D{desc.width, desc.height, desc.depth});
    }
}