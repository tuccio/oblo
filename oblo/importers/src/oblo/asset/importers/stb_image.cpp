#include <oblo/asset/importers/stb_image.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/scene/assets/texture.hpp>

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace oblo::importers
{
    // TODO: Support for 16 bit, you kind of have to guess based on filename
    // TODO: We assume SRGB, I don't see a way of actually checking using stbi
    // TODO: HDR, should be supported, but probably we need to call the float functions

    namespace
    {
        bool load_to_texture(texture& out, const u8* image, int w, int h, int channels)
        {
            u32 vkFormat;

            switch (channels)
            {
            case 1:
                vkFormat = VK_FORMAT_R8_SRGB;
                break;

            case 2:
                vkFormat = VK_FORMAT_R8G8_SRGB;
                break;

            case 3:
                vkFormat = VK_FORMAT_R8G8B8_SRGB;
                break;

            case 4:
                vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
                break;

            default:
                return false;
            }

            out.allocate({
                .vkFormat = vkFormat,
                .width = u32(w),
                .height = u32(h),
                .depth = 1,
                .dimensions = 2,
                .numLevels = 1,
                .numLayers = 1,
                .numFaces = 1,
                .isArray = false,
            });

            const auto ktxData = out.get_data();
            std::memcpy(ktxData.data(), image, ktxData.size());

            return true;
        }

    }

    bool stb_image::load_from_file(texture& out, const std::filesystem::path& path)
    {
        int w, h;
        int channels;

        // TODO: Use stbi_load_from_file instead, and avoid transcoding strings
        const auto pathStr = path.string();

        auto* const image = stbi_load(pathStr.c_str(), &w, &h, &channels, STBI_default);

        if (!image)
        {
            return false;
        }

        const auto cleanup = finally([image] { free(image); });

        return load_to_texture(out, image, w, h, channels);
    }

    bool stb_image::load_from_memory(texture& out, const std::span<const std::byte> data)
    {
        int w, h;
        int channels;

        auto* const image = stbi_load_from_memory(reinterpret_cast<const u8*>(data.data()),
            int(data.size()),
            &w,
            &h,
            &channels,
            STBI_default);

        if (!image)
        {
            return false;
        }

        const auto cleanup = finally([image] { free(image); });

        return load_to_texture(out, image, w, h, channels);
    }

    bool stb_image::init(const importer_config& config, import_preview& preview)
    {
        m_source = config.sourceFile;
        auto& node = preview.nodes.emplace_back();
        node.name = m_source.filename().stem().string();
        node.type = get_type_id<texture>();

        return true;
    }

    bool stb_image::import(const import_context& ctx)
    {
        const auto& modelNodeConfig = ctx.importNodesConfig[0];

        if (!modelNodeConfig.enabled)
        {
            return true;
        }

        texture t;

        if (!load_from_file(t, m_source))
        {
            return false;
        }

        m_result.id = modelNodeConfig.id;
        m_result.name = ctx.nodes[0].name;
        m_result.data = any_asset{std::move(t)};

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