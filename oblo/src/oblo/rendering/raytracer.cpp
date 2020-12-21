#include <oblo/rendering/raytracer.hpp>

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/rendering/material.hpp>

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

        m_numTriangles += triangles.size();

        bvh.build(triangles);
        return index;
    }

    u32 raytracer::add_material(const material& material)
    {
        u32 index = narrow_cast<u32>(m_materials.size());
        m_materials.emplace_back(material);
        return index;
    }

    u32 raytracer::add_instance(const render_instance& instance)
    {
        u32 index = narrow_cast<u32>(m_instances.size());
        m_instances.emplace_back(instance);
        return index;
    }

    std::span<const triangle_container> raytracer::get_meshes() const
    {
        return m_meshes;
    }

    void raytracer::clear()
    {
        m_blas.clear();
        m_tlas.clear();

        m_meshes.clear();
        m_materials.clear();
        m_instances.clear();
        m_aabbs.clear();

        m_numTriangles = 0;
    }

    void raytracer::render_tile(
        raytracer_state& state, const camera& camera, u16 minX, u16 minY, u16 maxX, u16 maxY) const
    {
        if (m_tlas.empty())
        {
            return;
        }

        constexpr vec2 uvStart{-1.f, -1.f};
        const vec2 uvOffset{2.f / state.m_width, 2.f / state.m_height};

        vec2 uv;

        auto metrics = raytracer_metrics{};

        for (u16 y = minY; y < maxY; ++y)
        {
            uv.y = uvStart.y + uvOffset.y * y;
            vec3* pixelOut = state.m_radianceBuffer.data() + state.m_width * y + minX;

            for (u16 x = minX; x < maxX; ++x)
            {
                uv.x = uvStart.x + uvOffset.x * x;

                const auto ray = ray_cast(camera, uv);

                auto color = vec3{0.f, 0.f, 0.f};

                const auto allAabbs = m_aabbs.get_aabbs();
                const auto allIds = m_aabbs.get_ids();

                m_tlas.traverse(
                    ray,
                    [&](u32 firstIndex, u16 numPrimitives, f32& currentDistance) mutable
                    {
                        const auto aabbs = allAabbs.subspan(firstIndex, numPrimitives);
                        u32 currentIndex = firstIndex;

                        for (const auto& aabb : aabbs)
                        {
                            float t0, t1;

                            if (oblo::intersect(ray, aabb, currentDistance, t0, t1) && t0 < currentDistance)
                            {
                                ++metrics.numTestedObjects;
                                const auto instanceIndex = allIds[currentIndex];

                                const auto& instance = m_instances[instanceIndex];
                                const auto meshIndex = instance.mesh;
                                const auto materialIndex = instance.material;

                                bool bestResult = false;

                                m_blas[meshIndex].traverse(
                                    ray,
                                    [&ray, &container = m_meshes[meshIndex], &currentDistance, &metrics, &bestResult](
                                        u32 firstIndex,
                                        u16 numPrimitives,
                                        f32& distance)
                                    {
                                        metrics.numTestedTriangles += numPrimitives;
                                        triangle_container::hit_result outResult;

                                        const bool anyIntersection =
                                            container.intersect(ray, firstIndex, numPrimitives, distance, outResult);

                                        if (anyIntersection && distance < currentDistance)
                                        {
                                            bestResult = true;
                                            currentDistance = distance;
                                        }
                                    });

                                if (bestResult)
                                {
                                    color = m_materials[materialIndex].albedo + m_materials[materialIndex].emissive;
                                }
                            }

                            ++currentIndex;
                        }
                    });

                *pixelOut = color;

                ++pixelOut;
            }
        }

        {
            metrics.width = maxX;
            metrics.height = maxY;
            metrics.numObjects = m_aabbs.size();
            metrics.numTriangles = m_numTriangles;
            metrics.numPrimaryRays = maxX * maxY;
            state.m_metrics = metrics;
        }
    }

    void raytracer::rebuild_tlas()
    {
        m_aabbs.clear();

        const auto numInstances = m_instances.size();

        u32 id = 0;
        m_aabbs.reserve(numInstances);

        for (const auto& instance : m_instances)
        {
            const auto meshIndex = instance.mesh;
            const auto aabb = m_blas[meshIndex].get_bounds();
            m_aabbs.add({&aabb, 1}, id++);
        }

        m_tlas.build(m_aabbs);
    }

    const bvh& raytracer::get_tlas() const
    {
        return m_tlas;
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