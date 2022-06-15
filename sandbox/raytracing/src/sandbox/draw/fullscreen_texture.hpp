#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    class fullscreen_texture
    {
    public:
        fullscreen_texture();
        fullscreen_texture(const fullscreen_texture&) = delete;
        fullscreen_texture(fullscreen_texture&&) noexcept = delete;
        ~fullscreen_texture();

        fullscreen_texture& operator=(const fullscreen_texture&) = delete;
        fullscreen_texture& operator=(fullscreen_texture&&) noexcept = delete;

        void resize(u32 width, u32 height);
        void update_tile(const u8* bytes, u32 x, u32 y, u32 w, u32 h);

        void draw();

    private:
        u32 m_shader{0u};
        u32 m_texture{0u};
    };
}