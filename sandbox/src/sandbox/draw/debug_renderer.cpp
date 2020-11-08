#include <sandbox/draw/debug_renderer.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/math/line.hpp>
#include <oblo/math/triangle.hpp>
#include <oblo/math/vec3.hpp>
#include <sandbox/sandbox_state.hpp>

#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <vector>

#include <GL/glew.h>

namespace oblo
{
    namespace
    {
        class vertex_array
        {
        public:
            vertex_array()
            {
                glGenVertexArrays(1, &m_vao);
                glBindVertexArray(m_vao);

                glGenBuffers(1, &m_vbo);

                glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(0);

                glBindVertexArray(0);
            }

            vertex_array(const vertex_array&) = delete;

            vertex_array(vertex_array&& other) noexcept
            {
                std::swap(m_vbo, other.m_vbo);
                std::swap(m_vao, other.m_vao);
            }

            ~vertex_array()
            {
                if (m_vbo != 0)
                {
                    glDeleteBuffers(1, &m_vbo);
                    glDeleteVertexArrays(1, &m_vao);
                }
            }

            void upload_vertices(std::span<const std::byte> data)
            {
                glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
                glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_DYNAMIC_DRAW);
            }

            void bind()
            {
                glBindVertexArray(m_vao);
            }

            static void unbind()
            {
                glBindVertexArray(0);
            }

        private:
            GLuint m_vao{0};
            GLuint m_vbo{0};
        };

        class ssbo
        {
        public:
            ssbo()
            {
                glGenBuffers(1, &m_ssbo);
            }

            ~ssbo()
            {
                if (m_ssbo)
                {
                    glDeleteBuffers(1, &m_ssbo);
                }
            }

            void upload(std::span<const std::byte> data)
            {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
                glBufferData(GL_SHADER_STORAGE_BUFFER, data.size(), data.data(), GL_DYNAMIC_DRAW);
            }

            void bind(GLuint index)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, m_ssbo);
            }

            static void unbind()
            {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            }

        private:
            GLuint m_ssbo{0};
        };

        constexpr const char* s_vertexShader = R"(
            #version 430

            in vec3 in_Position;

            uniform mat4 view;
            uniform mat4 projection;
            
            void main()
            {
                gl_Position = projection * view * vec4(in_Position, 1.f);
            }
        )";

        constexpr const char* s_fragmentShader = R"(
            #version 430
            out vec4 color;

            layout(std430, binding = 0) buffer Primitives
            {
                vec4 colors[];
            };

            void main()
            {
                color = colors[gl_PrimitiveID];
            }
        )";
    }

    struct debug_renderer::impl
    {
        vertex_array trianglesArray;
        vertex_array linesArray;
        ssbo triangleColorsBuffer;
        ssbo lineColorsBuffer;

        std::vector<vec3> trianglesVertices;
        std::vector<float> trianglesColor;

        std::vector<vec3> linesVertices;
        std::vector<float> linesColor;

        sf::Shader shader;
    };

    debug_renderer::debug_renderer() : m_impl{std::make_unique<impl>()}
    {
        [[maybe_unused]] const auto loaded = m_impl->shader.loadFromMemory(s_vertexShader, s_fragmentShader);
        OBLO_ASSERT(loaded);
    }

    debug_renderer::~debug_renderer() = default;

    void debug_renderer::draw_triangles(std::span<const triangle> triangles, const vec3& color)
    {
        auto& vertices = m_impl->trianglesVertices;
        auto& colors = m_impl->trianglesColor;

        vertices.reserve(vertices.size() + triangles.size() * 3);
        colors.reserve(vertices.size() + triangles.size() * 4);

        for (const auto& triangle : triangles)
        {
            vertices.emplace_back(triangle.v[0]);
            vertices.emplace_back(triangle.v[1]);
            vertices.emplace_back(triangle.v[2]);

            colors.insert(colors.end(), {color.x, color.y, color.z, 1.f});
        }
    }

    void debug_renderer::draw_lines(std::span<const line> lines, const vec3& color)
    {
        auto& vertices = m_impl->linesVertices;
        auto& colors = m_impl->linesColor;

        vertices.reserve(vertices.size() + lines.size() * 2);
        colors.reserve(vertices.size() + lines.size() * 4);

        for (const auto& line : lines)
        {
            vertices.emplace_back(line.v[0]);
            vertices.emplace_back(line.v[1]);
            colors.insert(colors.end(), {color.x, color.y, color.z, 1.f});
        }
    }

    void debug_renderer::dispatch_draw(const sandbox_state& state)
    {
        const auto hasNoTriangles = m_impl->trianglesVertices.empty();
        const auto hasNoLines = m_impl->linesVertices.empty();

        if (hasNoTriangles && hasNoLines)
        {
            return;
        }

        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);

        sf::Shader::bind(&m_impl->shader);
        glBindAttribLocation(m_impl->shader.getNativeHandle(), 0, "in_Position");

        const auto near = state.camera.near;
        const auto far = state.camera.far;
        const auto fx = std::tan(pi * .5f - float(state.camera.fovx) * .5f);
        const auto fy = std::tan(pi * .5f - float(state.camera.fovy) * .5f);
        const auto invRange = 1 / (near - far);

        const auto& eye = state.camera.position;
        const auto z = -state.camera.direction;
        const auto& y = state.camera.up;
        const auto x = y.cross(z);

        // clang-format off
        const float view[] = {
            x.x, y.x, z.x, 0.f,
            x.y, y.y, z.y, 0.f,
            x.z, y.z, z.z, 0.f,
            -x.dot(eye), -y.dot(eye), -z.dot(eye), 1.f
        };

        const float projection[] = {
            fx, 0.f, 0.f, 0.f,
            0.f, fy, 0.f, 0.f,
            0.f, 0.f, (far + near) * invRange,  -1.f,
            0.f, 0.f, 2.f * far * near * invRange, 0.f
        };
        // clang-format on

        m_impl->shader.setUniform("view", sf::Glsl::Mat4{view});
        m_impl->shader.setUniform("projection", sf::Glsl::Mat4{projection});

        if (!hasNoTriangles)
        {
            m_impl->triangleColorsBuffer.upload(std::as_bytes(std::span{m_impl->trianglesColor}));
            m_impl->triangleColorsBuffer.bind(0);

            m_impl->trianglesArray.bind();
            m_impl->trianglesArray.upload_vertices(std::as_bytes(std::span{m_impl->trianglesVertices}));

            glDrawArrays(GL_TRIANGLES, 0, m_impl->trianglesVertices.size());
        }

        if (!hasNoLines)
        {
            m_impl->lineColorsBuffer.upload(std::as_bytes(std::span{m_impl->linesColor}));
            m_impl->lineColorsBuffer.bind(0);

            m_impl->linesArray.bind();
            m_impl->linesArray.upload_vertices(std::as_bytes(std::span{m_impl->linesVertices}));

            glDrawArrays(GL_LINES, 0, m_impl->linesVertices.size());
        }

        ssbo::unbind();
        vertex_array::unbind();
        sf::Shader::bind(nullptr);

        m_impl->trianglesVertices.clear();
        m_impl->trianglesColor.clear();

        m_impl->linesVertices.clear();
        m_impl->linesColor.clear();
    }
}