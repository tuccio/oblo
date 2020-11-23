#pragma once

#include <oblo/acceleration/aabb_container.hpp>
#include <oblo/acceleration/bvh.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/rendering/camera.hpp>

#include <span>
#include <vector>

namespace oblo
{
    class raytracer_state;
    class triangle_container;

    class raytracer
    {
    public:
        raytracer();
        raytracer(const raytracer&) = delete;
        raytracer(raytracer&&) noexcept = delete;

        ~raytracer();

        raytracer& operator=(const raytracer&) = delete;
        raytracer& operator=(raytracer&&) noexcept = delete;

        void reserve(u32 numMeshes);

        u32 add_mesh(triangle_container triangles);

        void clear();

        void render_debug(raytracer_state& state, const camera& camera) const;

        void rebuild_tlas();

    private:
        bvh m_tlas;
        aabb_container m_aabbs;

        std::vector<bvh> m_blas;
        std::vector<triangle_container> m_meshes;
    };

    class raytracer_state
    {
    public:
        raytracer_state() = default;
        raytracer_state(u16 width, u16 height);
        raytracer_state(raytracer_state&&) noexcept = default;
        raytracer_state& operator=(raytracer_state&&) noexcept = default;

        void resize(u16 width, u16 height);

        u16 get_width() const
        {
            return m_width;
        }

        u16 get_height() const
        {
            return m_height;
        }

        std::span<const vec3> get_radiance_buffer() const
        {
            return m_radianceBuffer;
        }

    private:
        friend class raytracer;

        u16 m_width{0};
        u16 m_height{0};
        std::vector<vec3> m_radianceBuffer;
    };
}