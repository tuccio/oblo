#pragma once

#include <memory>
#include <span>

namespace oblo
{
    class raytracer_state;

    struct line;
    struct sandbox_state;
    struct triangle;
    struct vec3;

    class debug_renderer
    {
    public:
        debug_renderer();
        debug_renderer(const debug_renderer&) = delete;
        debug_renderer(debug_renderer&&) noexcept = delete;
        ~debug_renderer();

        debug_renderer& operator=(const debug_renderer&) = delete;
        debug_renderer& operator=(debug_renderer&&) noexcept = delete;

        void draw_triangles(std::span<const triangle> triangles, const vec3& color);
        void draw_lines(std::span<const line> lines, const vec3& color);

        void dispatch_draw(const sandbox_state& state);

        void clear();

    private:
        struct impl;
        std::unique_ptr<impl> m_impl;
    };
}