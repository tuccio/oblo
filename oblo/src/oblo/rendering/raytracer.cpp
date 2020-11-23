#include <oblo/rendering/raytracer.hpp>

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>

namespace oblo
{
    raytracer::raytracer() = default;

    raytracer::~raytracer() = default;

    void raytracer::reserve(u32 numMeshes)
    {
        m_blas.reserve(numMeshes);
        m_meshes.reserve(numMeshes);
    }

    u32 raytracer::add_mesh(triangle_container inTriangles)
    {
        u32 index = narrow_cast<u32>(m_meshes.size());
        auto& triangles = m_meshes.emplace_back(std::move(inTriangles));
        auto& bvh = m_blas.emplace_back();

        bvh.build(triangles);
        return index;
    }

    void raytracer::clear()
    {
        m_blas.clear();
        m_meshes.clear();
    }

    void raytracer::render_debug(raytracer_state& state, const camera& camera) const
    {
        const auto w = state.m_width;
        const auto h = state.m_height;

        constexpr vec2 uvStart{-1.f, -1.f};
        const vec2 uvOffset{2.f / w, 2.f / h};

        vec2 uv = uvStart;
        vec3* pixelOut = state.m_radianceBuffer.data();

        for (u16 y = 0; y < h; ++y)
        {
            uv.x = uvStart.x;

            for (u16 x = 0; x < w; ++x)
            {
                const auto ray = ray_cast(camera, uv);

                auto color = vec3{0.f, 0.f, 0.f};

                for (std::size_t i = 0; i < m_meshes.size(); ++i)
                {
                    f32 distance;
                    triangle_container::hit_result hitResult;

                    if (m_blas[i].intersect(ray, m_meshes[i], distance, hitResult))
                    {
                        color = vec3{1.f, 0.f, 0.f};
                        break;
                    }
                }

                *pixelOut = color;

                uv.x += uvOffset.x;
                ++pixelOut;
            }

            uv.y += uvOffset.y;
        }
    }

    raytracer_state::raytracer_state(u16 width, u16 height) : m_width{width}, m_height{height}
    {
        if (width > 0 && height > 0)
        {
            m_radianceBuffer.resize(width * height);
        }
    }

    void raytracer_state::resize(u16 width, u16 height)
    {
        m_width = width;
        m_height = height;
        m_radianceBuffer.resize(width * height);
    }
}