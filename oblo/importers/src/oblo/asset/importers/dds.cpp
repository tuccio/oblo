#include <oblo/asset/importers/dds.hpp>

#include <oblo/asset/import/import_config.hpp>
#include <oblo/asset/import/import_context.hpp>
#include <oblo/asset/import/import_preview.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/log/log.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/scene/resources/texture_format.hpp>

#include <bit>

static_assert(std::endian::native == std::endian::little, "DDS files are little-endian");

namespace oblo::importers
{
    namespace
    {
        consteval u32 make_fourcc(string_view str)
        {
            OBLO_ASSERT(str.size() == 4);
            return (str[0] | str[1] << 8 | str[2] << 16 | str[3] << 24);
        }

        // See: https://github.com/microsoft/DirectXTex/blob/main/DirectXTex/DDS.h
        struct dds_pixelformat
        {
            u32 size;
            u32 flags;
            u32 fourCC;
            u32 RGBBitCount;
            u32 RBitMask;
            u32 GBitMask;
            u32 BBitMask;
            u32 ABitMask;
        };

        struct dds_header
        {
            u32 size;
            u32 flags;
            u32 height;
            u32 width;
            u32 pitchOrLinearSize;
            u32 depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
            u32 mipMapCount;
            u32 reserved1[11];
            dds_pixelformat ddspf;
            u32 caps;
            u32 caps2;
            u32 caps3;
            u32 caps4;
            u32 reserved2;
        };

        enum class dds_format : u32
        {
            r10g10b10a2_unorm = 24,
            r8g8b8a8_unorm = 28,
            r8g8b8a8_unorm_srgb = 29,
            r16_unorm = 56,
            bc1_unorm = 71,
            bc1_unorm_srgb = 72,
            bc2_unorm = 74,
            bc2_unorm_srgb = 75,
            bc3_unorm = 77,
            bc3_unorm_srgb = 78,
            bc4_unorm = 80,
            bc5_unorm = 83,
            b8g8r8a8_unorm = 87,
            b8g8r8x8_unorm = 88,
            b8g8r8a8_unorm_srgb = 91,
            b8g8r8x8_unorm_srgb = 93,
            bc6h_uf16 = 95,
            bc6h_sf16 = 96,
            bc7_unorm = 98,
            bc7_unorm_srgb = 99
        };

        enum class dds_resource_dimension : u32
        {
            texture1d = 2,
            texture2d = 3,
            texture3d = 4,
        };

        struct dds_header_dxt10
        {
            dds_format dxgiFormat;
            dds_resource_dimension resourceDimension;
            u32 miscFlag; // see D3D11_RESOURCE_MISC_FLAG
            u32 arraySize;
            u32 miscFlags2; // see DDS_MISC_FLAGS2
        };

        struct dds_flags
        {
            enum flags : u32
            {
                fourcc = 0x00000004,        // DDPF_FOURCC
                rgb = 0x00000040,           // DDPF_RGB
                rgba = 0x00000041,          // DDPF_RGB | DDPF_ALPHAPIXELS
                luminance = 0x00020000,     // DDPF_LUMINANCE
                luminancea = 0x00020001,    // DDPF_LUMINANCE | DDPF_ALPHAPIXELS
                alphapixels = 0x00000001,   // DDPF_ALPHAPIXELS
                alpha = 0x00000002,         // DDPF_ALPHA
                pal8 = 0x00000020,          // DDPF_PALETTEINDEXED8
                pal8a = 0x00000021,         // DDPF_PALETTEINDEXED8 | DDPF_ALPHAPIXELS
                bumpluminance = 0x00040000, // DDPF_BUMPLUMINANCE
                bumpdudv = 0x00080000,      // DDPF_BUMPDUDV
                bumpdudva = 0x00080001      // DDPF_BUMPDUDV | DDPF_ALPHAPIXELS
            };
        };

        static_assert(sizeof(dds_pixelformat) == 32, "DDS pixel format size mismatch");
        static_assert(sizeof(dds_header) == 124, "DDS Header size mismatch");
        static_assert(sizeof(dds_header_dxt10) == 20, "DDS DX10 Extended Header size mismatch");

        texture_format convert_format(dds_format format)
        {
            switch (format)
            {
            case dds_format::r10g10b10a2_unorm:
                return texture_format::a2r10g10b10_unorm_pack32;
            case dds_format::r8g8b8a8_unorm:
                return texture_format::r8g8b8a8_unorm;
            case dds_format::r8g8b8a8_unorm_srgb:
                return texture_format::r8g8b8a8_srgb;
            case dds_format::r16_unorm:
                return texture_format::r16_unorm;
            case dds_format::bc1_unorm:
                return texture_format::bc1_rgb_unorm_block;
            case dds_format::bc1_unorm_srgb:
                return texture_format::bc1_rgb_srgb_block;
            case dds_format::bc2_unorm:
                return texture_format::bc2_unorm_block;
            case dds_format::bc2_unorm_srgb:
                return texture_format::bc2_srgb_block;
            case dds_format::bc3_unorm:
                return texture_format::bc3_unorm_block;
            case dds_format::bc3_unorm_srgb:
                return texture_format::bc3_srgb_block;
            case dds_format::bc4_unorm:
                return texture_format::bc4_unorm_block;
            case dds_format::bc5_unorm:
                return texture_format::bc5_unorm_block;
            case dds_format::b8g8r8a8_unorm:
                return texture_format::b8g8r8a8_unorm;
            case dds_format::b8g8r8x8_unorm:
                return texture_format::b8g8r8_unorm;
            case dds_format::b8g8r8a8_unorm_srgb:
                return texture_format::b8g8r8a8_srgb;
            case dds_format::b8g8r8x8_unorm_srgb:
                return texture_format::b8g8r8_srgb;
            case dds_format::bc6h_uf16:
                return texture_format::bc6h_ufloat_block;
            case dds_format::bc6h_sf16:
                return texture_format::bc6h_sfloat_block;
            case dds_format::bc7_unorm:
                return texture_format::bc7_unorm_block;
            case dds_format::bc7_unorm_srgb:
                return texture_format::bc7_srgb_block;
            default:
                OBLO_ASSERT(false);
                return texture_format::undefined;
            }
        }
    }

    bool dds::init(const import_config& config, import_preview& preview)
    {
        m_source = config.sourceFile;
        auto& node = preview.nodes.emplace_back();
        node.name = filesystem::stem(config.sourceFile).as<string>();
        node.artifactType = resource_type<texture>;

        return true;
    }

    bool dds::import(import_context ctx)
    {
        const auto& modelNodeConfig = ctx.get_import_node_configs()[0];

        if (!modelNodeConfig.enabled)
        {
            return true;
        }

        const filesystem::file_ptr file{filesystem::open_file(m_source, "rb")};

        if (!file)
        {
            log::debug("Failed to open {}", m_source);
            return false;
        }

        u32 magic;

        if (std::fread(&magic, 1, sizeof(u32), file.get()) != sizeof(u32) || magic != make_fourcc("DDS "))
        {
            log::debug("Invalid DDS: {}", m_source);
            return false;
        }

        dds_header ddsHeader;

        if (std::fread(&ddsHeader, 1, sizeof(dds_header), file.get()) != sizeof(dds_header))
        {
            log::debug("Failed to read header: {}", m_source);
            return false;
        }

        if (ddsHeader.width % 4 != 0 || ddsHeader.height % 4 != 0)
        {
            log::debug("DDS requires width and height to be multiples of 4, but input image is {}x{}",
                ddsHeader.width,
                ddsHeader.height);
        }

        texture_desc desc{
            .width = ddsHeader.width,
            .height = ddsHeader.height,
            .depth = max(1u, ddsHeader.depth),
            .dimensions = 2, // DXT10 extension may have dimensions
            .numLevels = max(1u, ddsHeader.mipMapCount),
            .numLayers = 1,
            .numFaces = 1, // It would be 6 for cubemaps
            .isArray = false,
        };

        dds_header_dxt10 dx10Header{};

        if (ddsHeader.ddspf.flags & dds_flags::fourcc) // Compressed formats
        {
            // u32 dataSize = ddsHeader.pitchOrLinearSize;

            switch (ddsHeader.ddspf.fourCC)
            {
            case make_fourcc("DXT1"):

                desc.vkFormat = ddsHeader.ddspf.flags & dds_flags::rgb ? texture_format::bc1_rgb_unorm_block
                                                                       : texture_format::bc1_rgba_unorm_block;
                break;

            case make_fourcc("DXT3"):
                desc.vkFormat = texture_format::bc2_unorm_block;
                break;

            case make_fourcc("DXT5"):
                desc.vkFormat = texture_format::bc3_unorm_block;
                break;

            case make_fourcc("BC4U"):
                desc.vkFormat = texture_format::bc4_unorm_block;
                break;

            case make_fourcc("BC4S"):
                desc.vkFormat = texture_format::bc4_snorm_block;
                break;

            case make_fourcc("BC5U"):
                desc.vkFormat = texture_format::bc5_unorm_block;
                break;

            case make_fourcc("BC5S"):
                desc.vkFormat = texture_format::bc5_snorm_block;
                break;

            case make_fourcc("DX10"): {
                if (std::fread(&dx10Header, 1, sizeof(dds_header_dxt10), file.get()) != sizeof(dds_header_dxt10))
                {
                    log::debug("Failed to read DX10 header: {}", m_source);
                    return false;
                }

                desc.vkFormat = convert_format(dx10Header.dxgiFormat);

                if (desc.vkFormat == texture_format::undefined)
                {
                    log::debug("Unsupported DXGI type: {:#x}", u32(dx10Header.dxgiFormat));
                    return false;
                }

                desc.isArray = dx10Header.arraySize > 1;
                desc.numLayers = max(1u, dx10Header.arraySize);

                switch (dx10Header.resourceDimension)
                {
                case dds_resource_dimension::texture1d:
                    desc.dimensions = 1;
                    break;

                case dds_resource_dimension::texture2d:
                    desc.dimensions = 2;
                    break;

                case dds_resource_dimension::texture3d:
                    desc.dimensions = 3;
                    break;

                default:
                    log::debug("Unsupported dimension: {:#x}", u32(dx10Header.resourceDimension));
                    return false;
                }
            }

            break;

            default:
                log::debug("Unsupported format: {:#x}", ddsHeader.ddspf.fourCC);
                return false;
            }
        }

        texture out;

        if (!out.allocate(desc))
        {
            log::debug("Failed to allocate texture resource");
            return false;
        }

        for (u32 level = 0; level < desc.numLevels; ++level)
        {
            for (u32 layer = 0; layer < desc.numLayers; ++layer)
            {
                for (u32 face = 0; face < desc.numFaces; ++face)
                {
                    const std::span outData = out.get_data(level, face, layer);

                    if (std::fread(outData.data(), 1, outData.size(), file.get()) != outData.size())
                    {
                        log::debug("Failed to read level {} layer {} face {}", level, layer, face);
                    }
                }
            }
        }

        string_builder outPath;

        if (!out.save(ctx.get_output_path(modelNodeConfig.id, outPath)))
        {
            return false;
        }

        m_result.id = modelNodeConfig.id;
        m_result.type = resource_traits<texture>::uuid;
        m_result.name = ctx.get_import_nodes()[0].name;
        m_result.path = outPath.as<string>();

        return true;
    }

    file_import_results dds::get_results()
    {
        const auto count = u32(!m_result.id.is_nil());

        return {
            .artifacts = {&m_result, count},
            .sourceFiles = {&m_source, count},
            .mainArtifactHint = m_result.id,
        };
    }
}