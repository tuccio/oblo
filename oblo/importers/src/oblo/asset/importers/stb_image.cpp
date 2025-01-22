#include <oblo/asset/importers/stb_image.hpp>

#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/importers/image_processing.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/color.hpp>
#include <oblo/math/float.hpp>
#include <oblo/math/power_of_two.hpp>
#include <oblo/scene/assets/texture.hpp>

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace oblo::importers
{
    // TODO: Support for 16 bit, you kind of have to guess based on filename
    // TODO: Seems like STB converts srgb to linear
    // TODO: HDR, should be supported, but probably we need to call the float functions

    namespace
    {
        bool load_to_texture(texture& out, const u8* image, int w, int h, u32 vkFormat)
        {
            const u32 width = u32(w);
            const u32 height = u32(h);
            const u32 numLevels = log2_round_down_power_of_two(min(width, height));

            out.allocate({
                .vkFormat = vkFormat,
                .width = u32(w),
                .height = u32(h),
                .depth = 1,
                .dimensions = 2,
                .numLevels = numLevels,
                .numLayers = 1,
                .numFaces = 1,
                .isArray = false,
            });

            const auto highestMip = out.get_data(0, 0, 0);
            std::memcpy(highestMip.data(), image, highestMip.size());

            for (u32 mipLevel = 1; mipLevel < numLevels; ++mipLevel)
            {
                const std::span<const byte> previous = out.get_data(mipLevel - 1, 0, 0);
                const std::span<byte> current = out.get_data(mipLevel, 0, 0);

                const u32 prevMipWidth = width >> (mipLevel - 1);
                const u32 prevMipHeight = height >> (mipLevel - 1);

                const u32 mipWidth = width >> mipLevel;
                const u32 mipHeight = height >> mipLevel;

                using namespace image_processing;

                switch (vkFormat)
                {
                case VK_FORMAT_R8_UNORM:
                    parallel_for_each_pixel(image_view_r<u8>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_r<const u8>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R8G8_UNORM:
                    parallel_for_each_pixel(image_view_rg<u8>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_rg<const u8>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R8G8B8_UNORM:
                    parallel_for_each_pixel(image_view_rgb<u8>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_rgb<const u8>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R8G8B8A8_UNORM:
                    parallel_for_each_pixel(image_view_rgba<u8>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_rgba<const u8>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R32_SFLOAT:
                    parallel_for_each_pixel(image_view_r<f32>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_r<const f32>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R32G32_SFLOAT:
                    parallel_for_each_pixel(image_view_rg<f32>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_rg<const f32>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R32G32B32_SFLOAT:
                    parallel_for_each_pixel(image_view_rgb<f32>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_rgb<const f32>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                case VK_FORMAT_R32G32B32A32_SFLOAT:
                    parallel_for_each_pixel(image_view_rgba<f32>{current, mipWidth, mipHeight},
                        box_filter_2x2{
                            linear_color_tag{},
                            image_view_rgba<const f32>{previous, prevMipWidth, prevMipHeight},
                        });
                    break;

                default:
                    unreachable();
                }
            }

            if (vkFormat == VK_FORMAT_R8G8B8_UNORM)
            {
                // Convert RGB8 to RGBA8
                texture withAlpha;

                withAlpha.allocate({
                    .vkFormat = VK_FORMAT_R8G8B8A8_UNORM,
                    .width = u32(w),
                    .height = u32(h),
                    .depth = 1,
                    .dimensions = 2,
                    .numLevels = numLevels,
                    .numLayers = 1,
                    .numFaces = 1,
                    .isArray = false,
                });

                for (u32 mipLevel = 0; mipLevel < numLevels; ++mipLevel)
                {
                    const u32 mipWidth = width >> mipLevel;
                    const u32 mipHeight = height >> mipLevel;

                    using namespace image_processing;

                    const image_view_rgb<u8> rgb8{out.get_data(mipLevel, 0, 0), mipWidth, mipHeight};
                    const image_view_rgba<u8> rgba8{withAlpha.get_data(mipLevel, 0, 0), mipWidth, mipHeight};

                    parallel_for_each_pixel(rgba8,
                        [&rgb8](u32 i, u32 j, image_view_rgba<u8>::pixel_view pixel)
                        {
                            const auto source = rgb8.at(i, j);

                            for (u32 k = 0; k < 3; ++k)
                            {
                                pixel[k] = source[k];
                                pixel[3] = u8{0xffu};
                            }
                        });
                }

                out = std::move(withAlpha);
            }
            else if (vkFormat == VK_FORMAT_R32G32B32_SFLOAT)
            {
                // Convert RGB32F to RGBA32F
                texture withAlpha;

                withAlpha.allocate({
                    .vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .width = u32(w),
                    .height = u32(h),
                    .depth = 1,
                    .dimensions = 2,
                    .numLevels = numLevels,
                    .numLayers = 1,
                    .numFaces = 1,
                    .isArray = false,
                });

                for (u32 mipLevel = 0; mipLevel < numLevels; ++mipLevel)
                {
                    const u32 mipWidth = width >> mipLevel;
                    const u32 mipHeight = height >> mipLevel;

                    using namespace image_processing;

                    const image_view_rgb<f32> rgb{out.get_data(mipLevel, 0, 0), mipWidth, mipHeight};
                    const image_view_rgba<f32> rgba{withAlpha.get_data(mipLevel, 0, 0), mipWidth, mipHeight};

                    parallel_for_each_pixel(rgba,
                        [&rgb](u32 i, u32 j, image_view_rgba<f32>::pixel_view pixel)
                        {
                            const auto source = rgb.at(i, j);

                            for (u32 k = 0; k < 3; ++k)
                            {
                                pixel[k] = source[k];
                                pixel[3] = 1.f;
                            }
                        });
                }

                out = std::move(withAlpha);
            }

            return true;
        }

        using image_ptr = std::unique_ptr<u8[], decltype([](u8* ptr) { free(ptr); })>;

        image_ptr load_from_file(cstring_view path, int& w, int& h, int& channels, bool isHDR)
        {
            image_ptr ptr;

            if (isHDR)
            {
                ptr.reset(reinterpret_cast<stbi_uc*>(stbi_loadf(path.c_str(), &w, &h, &channels, STBI_default)));
            }
            else
            {
                ptr.reset(stbi_load(path.c_str(), &w, &h, &channels, STBI_default));
            }

            return ptr;
        }
    }

    bool stb_image::init(const import_config& config, import_preview& preview)
    {
        m_source = config.sourceFile;
        auto& node = preview.nodes.emplace_back();
        node.name = filesystem::stem(config.sourceFile).as<string>();
        node.type = get_type_id<texture>();

        return true;
    }

    bool stb_image::import(import_context ctx)
    {
        const auto& modelNodeConfig = ctx.get_import_node_configs()[0];

        if (!modelNodeConfig.enabled)
        {
            return true;
        }

        bool isHDR = false;

        // TODO (#65): Case-insensitive utf8 checks
        if (const auto f = string_view{m_source}; f.ends_with(".hdr") || f.ends_with(".HDR"))
        {
            isHDR = true;
        }

        int w, h;
        int channels;

        image_ptr image = load_from_file(m_source, w, h, channels, isHDR);

        if (!image)
        {
            return false;
        }

        const auto& settings = ctx.get_settings();

        if (const auto swizzle = settings.find_child(settings.get_root(), "swizzle");
            swizzle != data_node::Invalid && settings.is_array(swizzle))
        {
            const auto swizzleCount = settings.children_count(swizzle);

            if (swizzleCount == 0 || swizzleCount > 4)
            {
                return false;
            }

            u32 swizzleChannels[4];

            u32 previousElement = data_node::Invalid;

            for (u32 i = 0; i < swizzleCount; ++i)
            {
                previousElement = settings.child_next(swizzle, previousElement);

                const auto e = previousElement;
                const auto c = settings.read_u32(e);

                if (!c || *c >= u32(channels))
                {
                    return false;
                }

                swizzleChannels[i] = *c;
            }

            const u32 inRowPitch = u32(round_up_multiple(w * channels, 4));
            const u32 outRowPitch = round_up_multiple(w * swizzleCount, 4u);

            image_ptr swizzled{static_cast<u8*>(malloc(outRowPitch * h))};

            for (u32 i = 0; i < u32(h); ++i)
            {
                const u32 inRowOffset = inRowPitch * i;
                const u32 outRowOffset = outRowPitch * i;

                for (u32 j = 0; j < u32(w); ++j)
                {
                    const u32 inOffset = inRowOffset + u32(channels) * j;
                    const u32 outOffset = outRowOffset + swizzleCount * j;

                    for (u32 k = 0; k < swizzleCount; ++k)
                    {
                        swizzled[outOffset + k] = image[inOffset + swizzleChannels[k]];
                    }
                }
            }

            image = std::move(swizzled);
            channels = int(swizzleCount);
        }

        texture t;

        constexpr u32 u8Formats[] = {
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R8G8_UNORM,
            VK_FORMAT_R8G8B8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
        };

        constexpr u32 f32Formats[] = {
            VK_FORMAT_R32_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT,
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
        };

        const u32 vkFormat = isHDR ? f32Formats[channels - 1] : u8Formats[channels - 1];

        if (!load_to_texture(t, image.get(), w, h, vkFormat))
        {
            return false;
        }

        string_builder outPath;

        if (!t.save(ctx.get_output_path(modelNodeConfig.id, outPath)))
        {
            return false;
        }

        m_result.id = modelNodeConfig.id;
        m_result.type = get_type_id<texture>();
        m_result.name = ctx.get_import_nodes()[0].name;
        m_result.path = outPath.as<string>();

        return true;
    }

    file_import_results stb_image::get_results()
    {
        const auto count = u32(!m_result.id.is_nil());

        return {
            .artifacts = {&m_result, count},
            .sourceFiles = {&m_source, count},
            .mainArtifactHint = m_result.id,
        };
    }
}