#include <oblo/scene/assets/texture.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/file_utility.hpp>
#include <oblo/math/vec2u.hpp>

#include <vulkan/vulkan_core.h>

#include <ktx.h>
#include <ktxvulkan.h>

namespace oblo
{
    namespace
    {
        ktxTexture* to_ktx(void* ptr)
        {
            return reinterpret_cast<ktxTexture*>(ptr);
        }

        void* to_impl(ktxTexture2* ktx)
        {
            return reinterpret_cast<ktxTexture*>(ktx);
        }

        void* to_impl(ktxTexture* ktx)
        {
            return ktx;
        }
    }

    texture::texture(texture&& other) noexcept
    {
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }

    texture& texture::operator=(texture&& other) noexcept
    {
        deallocate();
        m_impl = other.m_impl;
        other.m_impl = nullptr;
        return *this;
    }

    texture::~texture()
    {
        deallocate();
    }

    bool texture::allocate(const texture_desc& desc)
    {
        deallocate();

        ktxTexture2* newKtx{};

        ktxTextureCreateInfo createInfo{
            .vkFormat = desc.vkFormat,
            .baseWidth = desc.width,
            .baseHeight = desc.height,
            .baseDepth = desc.depth,
            .numDimensions = desc.dimensions,
            .numLevels = desc.numLevels,
            .numLayers = desc.numLayers,
            .numFaces = desc.numFaces,
            .isArray = desc.isArray,
            .generateMipmaps = false,
        };

        const auto err =
            ktxTexture2_Create(&createInfo, ktxTextureCreateStorageEnum::KTX_TEXTURE_CREATE_ALLOC_STORAGE, &newKtx);

        if (err != ktx_error_code_e::KTX_SUCCESS)
        {
            return false;
        }

        m_impl = to_impl(newKtx);
        return true;
    }

    void texture::deallocate()
    {
        if (auto* const ktx = to_ktx(m_impl))
        {
            ktxTexture_Destroy(ktx);
            m_impl = nullptr;
        }
    }

    vec2u texture::get_resolution() const
    {
        auto* const t = to_ktx(m_impl);
        return {t->baseWidth, t->baseHeight};
    }

    std::span<std::byte> texture::get_data()
    {
        auto* const t = to_ktx(m_impl);
        u8* const data = ktxTexture_GetData(t);
        const auto dataSize = ktxTexture_GetDataSize(t);
        return std::span<std::byte>{reinterpret_cast<std::byte*>(data), dataSize};
    }

    std::span<const std::byte> texture::get_data() const
    {
        auto* const t = to_ktx(m_impl);
        u8* const data = ktxTexture_GetData(t);
        const auto dataSize = ktxTexture_GetDataSize(t);
        return std::span<const std::byte>{reinterpret_cast<std::byte*>(data), dataSize};
    }

    std::span<std::byte> texture::get_data(u32 level, u32 face, u32 layer)
    {
        auto* const t = to_ktx(m_impl);
        ktx_size_t offset;

        // It looks like the names of the parameters in the macro are wrong: level, layer, face is the correct order
        const ktx_error_code_e result = ktxTexture_GetImageOffset(t, level, layer, face, &offset);

        OBLO_ASSERT(result == ktx_error_code_e::KTX_SUCCESS);

        if (result != ktx_error_code_e::KTX_SUCCESS)
        {
            return {};
        }

        const auto size = ktxTexture_GetImageSize(t, level);
        return get_data().subspan(offset, size);
    }

    std::span<const std::byte> texture::get_data(u32 level, u32 face, u32 layer) const
    {
        auto* const t = to_ktx(m_impl);
        ktx_size_t offset;
        // It looks like the names of the parameters in the macro are wrong: level, layer, face is the correct order
        const ktx_error_code_e result = ktxTexture_GetImageOffset(t, level, layer, face, &offset);

        OBLO_ASSERT(result == ktx_error_code_e::KTX_SUCCESS);

        if (result != ktx_error_code_e::KTX_SUCCESS)
        {
            return {};
        }

        const auto size = ktxTexture_GetImageSize(t, level);
        return get_data().subspan(offset, size);
    }

    u32 texture::get_row_pitch(u32 level) const
    {
        return ktxTexture_GetRowPitch(to_ktx(m_impl), level);
    }

    u32 texture::get_offset(u32 level, u32 face, u32 layer) const
    {
        auto* const t = to_ktx(m_impl);

        ktx_size_t offset;
        // It looks like the names of the parameters in the macro are wrong: level, layer, face is the correct order
        [[maybe_unused]] const ktx_error_code_e result = ktxTexture_GetImageOffset(t, level, layer, face, &offset);

        OBLO_ASSERT(result == ktx_error_code_e::KTX_SUCCESS);

        return u32(offset);
    }

    bool texture::save(const std::filesystem::path& path) const
    {
        const file_ptr f{open_file(path, "wb")};

        if (!f)
        {
            return false;
        }

        return KTX_SUCCESS == ktxTexture_WriteToStdioStream(to_ktx(m_impl), f.get());
    }

    bool texture::load(const std::filesystem::path& path)
    {
        deallocate();

        ktxTexture* newKtx{};

        const file_ptr f{open_file(path, "rb")};

        if (!f)
        {
            return false;
        }

        const auto err = ktxTexture_CreateFromStdioStream(f.get(),
            ktxTextureCreateStorageEnum::KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            &newKtx);

        if (err != KTX_SUCCESS)
        {
            return false;
        }

        m_impl = to_impl(newKtx);

        return true;
    }

    texture_desc texture::get_description() const
    {
        auto* const t = to_ktx(m_impl);

        return {
            .vkFormat = u32(ktxTexture_GetVkFormat(t)),
            .width = t->baseWidth,
            .height = t->baseHeight,
            .depth = t->baseDepth,
            .dimensions = t->numDimensions,
            .numLevels = t->numLevels,
            .numLayers = t->numLayers,
            .numFaces = t->numFaces,
            .isArray = t->isArray,
        };
    }
}