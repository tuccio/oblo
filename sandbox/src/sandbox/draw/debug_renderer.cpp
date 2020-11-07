#include <sandbox/draw/debug_renderer.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/math/constants.hpp>
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
        struct triangles_draw_command
        {
            u32 offset;
            u32 numVertices;
        };

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

        constexpr const char* s_vertexShader = R"(
            #version 330

            in vec3 in_Position;

            uniform mat4 view;
            uniform mat4 projection;
            
            void main()
            {
                // gl_Position = projection * vec4(in_Position.x, in_Position.y - 1.f, in_Position.z - 1.f, 1.f);
                gl_Position = projection * view * vec4(in_Position, 1.f);
            }
        )";

        constexpr const char* s_fragmentShader = R"(
            #version 330
            out vec4 color;

            void main()
            {
                vec4 colors[6]; 
                colors[0] = vec4(1, 0, 0, 1);
                colors[1] = vec4(0, 1, 0, 1);
                colors[2] = vec4(0, 0, 1, 1);
                colors[3] = vec4(1, 1, 0, 1);
                colors[4] = vec4(1, 0, 1, 1);
                colors[5] = vec4(0, 1, 1, 1);
                color = colors[gl_PrimitiveID % 6];
            }
        )";
    }

    struct debug_renderer::impl
    {
        vertex_array trianglesArray;
        std::vector<vec3> trianglesVertices;
        std::vector<triangles_draw_command> trianglesDrawCommands;

        sf::Shader shader;
    };

    debug_renderer::debug_renderer() : m_impl{std::make_unique<impl>()}
    {
        [[maybe_unused]] const auto loaded = m_impl->shader.loadFromMemory(s_vertexShader, s_fragmentShader);
        OBLO_ASSERT(loaded);
    }

    debug_renderer::~debug_renderer() = default;

    void debug_renderer::draw(std::span<const triangle> triangles, const vec3& color)
    {
        auto& vertices = m_impl->trianglesVertices;
        const auto offset = vertices.size();

        vertices.reserve(vertices.size() + triangles.size() * 3);

        for (const auto& triangle : triangles)
        {
            vertices.emplace_back(triangle.v[0]);
            vertices.emplace_back(triangle.v[1]);
            vertices.emplace_back(triangle.v[2]);
        }

        m_impl->trianglesDrawCommands.push_back({narrow_cast<u32>(offset), 3 * narrow_cast<u32>(triangles.size())});
    }

    void debug_renderer::dispatch_draw(const sandbox_state& state)
    {
        if (m_impl->trianglesVertices.empty())
        {
            return;
        }

        glEnable(GL_DEPTH_TEST);

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
         float view[] = {
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

        m_impl->trianglesArray.bind();
        m_impl->trianglesArray.upload_vertices(std::as_bytes(std::span{m_impl->trianglesVertices}));

        glClear(GL_DEPTH_BUFFER_BIT);

        for (const auto& draw : m_impl->trianglesDrawCommands)
        {
            glDrawArrays(GL_TRIANGLES, draw.offset, draw.numVertices);
        }

        vertex_array::unbind();
        sf::Shader::bind(nullptr);

        m_impl->trianglesVertices.clear();
        m_impl->trianglesDrawCommands.clear();
    }
}