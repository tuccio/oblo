#include <sandbox/draw/fullscreen_texture.hpp>

#include <GL/glew.h>
#include <oblo/core/debug.hpp>
#include <sandbox/draw/utility.hpp>

namespace oblo
{
    namespace
    {
        constexpr auto s_vertexShader = R"(
            #version 450

            out vec2 uv;

            void main()
            {
                const vec2[4] position = vec2[4] (
                    vec2(-1, -1),
                    vec2(1, -1),
                    vec2(-1, 1),
                    vec2(1, 1));
                    
                const vec2 pos = position[gl_VertexID];
                gl_Position = vec4(pos, 0.0, 1.0);
                uv = pos * vec2(0.5, -0.5) + 0.5;
            }
        )";

        constexpr auto s_fragmentShader = R"(
            #version 450

            in vec2 uv;
            out vec4 color;

            layout (location = 0) uniform sampler2D tex;

            void main()
            {
                color.rgb = texture(tex, uv).rgb;
            }
        )";
    }

    fullscreen_texture::fullscreen_texture()
    {
        m_shader = compile_vert_frag_program(s_vertexShader, s_fragmentShader);
        OBLO_ASSERT(m_shader);
        glGenTextures(1, &m_texture);
    }

    fullscreen_texture::~fullscreen_texture()
    {
        if (m_shader)
        {
            glDeleteShader(m_shader);
        }

        if (m_texture)
        {
            glDeleteTextures(1, &m_texture);
        }
    }

    void fullscreen_texture::resize(u32 width, u32 height)
    {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void fullscreen_texture::update_tile(const u8* bytes, u32 x, u32 y, u32 w, u32 h)
    {
        glTextureSubImage2D(m_texture, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    }

    void fullscreen_texture::draw()
    {
        constexpr auto samplerLocation = 0;

        glUseProgram(m_shader);

        glUniform1i(samplerLocation, 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texture);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }
}