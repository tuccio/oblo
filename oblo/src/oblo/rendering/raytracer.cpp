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
        if (m_tlas.empty())
        {
            return;
        }
        
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

                const auto allAabbs = m_aabbs.get_aabbs();
                const auto allIds = m_aabbs.get_ids();

                m_tlas.traverse(
                    ray,
                    [&](u32 firstIndex, u16 numPrimitives, f32& distance) mutable
                    {
                        const auto aabbs = allAabbs.subspan(firstIndex, numPrimitives);
                        u32 currentIndex = firstIndex;

                        for (const auto& aabb : aabbs)
                        {
                            float t0, t1;

                            if (oblo::intersect(ray, aabb, distance, t0, t1) && t0 < distance)
                            {
                                const auto id = allIds[currentIndex];

                                m_blas[id].traverse(
                                    ray,
                                    [&color, &ray, &container = m_meshes[id]](u32 firstIndex,
                                                                              u16 numPrimitives,
                                                                              f32& distance)
                                    {
                                        triangle_container::hit_result outResult;
                                        const bool anyIntersection =
                                            container.intersect(ray, firstIndex, numPrimitives, distance, outResult);

                                        if (anyIntersection)
                                        {
                                            color = vec3{0.f, 1.f, 0.f};
                                        }
                                    });
                            }

                            ++currentIndex;
                        }
                    });

                *pixelOut = color;

                uv.x += uvOffset.x;
                ++pixelOut;
            }

            uv.y += uvOffset.y;
        }
    }

    void raytracer::rebuild_tlas()
    {
        m_aabbs.clear();

        u32 id = 0;
        m_aabbs.reserve(m_blas.size());

        for (const auto& blas : m_blas)
        {
            const auto aabb = blas.get_bounds();
            m_aabbs.add({&aabb, 1}, id++);
        }

        m_tlas.build(m_aabbs);
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