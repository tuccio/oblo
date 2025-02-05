#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/scene/resources/traits.hpp>

#include <span>

namespace oblo
{
    enum class texture_format : u32;
    struct vec2u;

    struct texture_desc
    {
        texture_format vkFormat;
        u32 width;
        u32 height;
        u32 depth;
        u32 dimensions;
        u32 numLevels;
        u32 numLayers;
        /// Should be 6 for cubemaps, 1 in any other case.
        u32 numFaces;
        bool isArray;
    };

    class SCENE_API texture
    {
    public:
        texture() = default;
        texture(const texture&) = delete;
        texture(texture&&) noexcept;

        texture& operator=(const texture&) = delete;
        texture& operator=(texture&&) noexcept;

        ~texture();

        bool allocate(const texture_desc& desc);
        void deallocate();

        vec2u get_resolution() const;

        std::span<std::byte> get_data();
        std::span<const std::byte> get_data() const;

        std::span<std::byte> get_data(u32 level, u32 face, u32 layer);
        std::span<const std::byte> get_data(u32 level, u32 face, u32 layer) const;

        u32 get_row_pitch(u32 level) const;

        /// @brief Retrieves the element size, i.e. the texel for uncompressed textures, the block otherwise.
        u32 get_element_size() const;

        bool save(cstring_view path) const;
        bool load(cstring_view path);

        texture_desc get_description() const;

    private:
        void* m_impl{};
    };
}