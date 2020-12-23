#include <oblo/rendering/raytracer.hpp>

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/math/random.hpp>
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

        constexpr u32 numSamples = 4;

        constexpr vec2 uvStart{-1.f, -1.f};
        const vec2 uvOffset{2.f / state.m_width, 2.f / state.m_height};

        std::uniform_real_distribution<f32> jitterDistX{-1.f / state.m_width, 1.f / state.m_width};
        std::uniform_real_distribution<f32> jitterDistY{-1.f / state.m_height, 1.f / state.m_height};

        const auto accumulationBuffer = state.m_accumulationBuffer.data() + state.get_accumulation_offset(minX, minY);
        *accumulationBuffer += numSamples;
        state.m_metrics.numTotalSamples = *accumulationBuffer;

        for (u32 sample = 0; sample < numSamples; ++sample)
        {
            vec2 uv;

            for (u16 y = minY; y < maxY; ++y)
            {
                const auto baseY = uvStart.y + uvOffset.y * y;
                vec3* pixelOut = state.m_radianceBuffer.data() + state.m_width * y + minX;

                for (u16 x = minX; x < maxX; ++x)
                {
                    uv.y = baseY + jitterDistY(state.m_rng);
                    uv.x = uvStart.x + uvOffset.x * x + jitterDistX(state.m_rng);

                    const auto ray = ray_cast(camera, uv);
                    *pixelOut += compute_lighting_recursive(ray, state, 1);

                    ++pixelOut;
                }
            }
        }

        {
            const auto w = maxX - minX;
            const auto h = maxY - minY;
            state.m_metrics.width = w;
            state.m_metrics.height = h;
            state.m_metrics.numObjects = m_aabbs.size();
            state.m_metrics.numTriangles = m_numTriangles;
            state.m_metrics.numPrimaryRays = w * h;
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

    bool raytracer::intersect(const ray& ray, raytracer_result& out) const
    {
        const auto allAabbs = m_aabbs.get_aabbs();
        const auto allIds = m_aabbs.get_ids();

        bool found{false};

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
                        const auto instanceIndex = allIds[currentIndex];

                        const auto& instance = m_instances[instanceIndex];
                        const auto meshIndex = instance.mesh;
                        const auto materialIndex = instance.material;

                        u32 triangle;
                        bool bestResult = false;

                        m_blas[meshIndex].traverse(
                            ray,
                            [&, &container = m_meshes[meshIndex]](u32 firstIndex, u16 numPrimitives, f32& distance)
                            {
                                triangle_container::hit_result hitResult;

                                const bool anyIntersection =
                                    container.intersect(ray, firstIndex, numPrimitives, distance, hitResult);

                                if (anyIntersection && distance < currentDistance)
                                {
                                    bestResult = true;
                                    currentDistance = distance;
                                    triangle = hitResult.index;
                                }
                            });

                        if (bestResult)
                        {
                            found = true;
                            out.distance = currentDistance;
                            out.instance = instanceIndex;
                            out.mesh = meshIndex;
                            out.material = materialIndex;
                            out.triangle = triangle;
                        }
                    }

                    ++currentIndex;
                }
            });

        return found;
    }

    vec3 raytracer::compute_lighting_recursive(const ray& ray, raytracer_state& state, const u16 bounces) const
    {
        raytracer_result out{.metrics = &state.m_metrics};

        if (!intersect(ray, out))
        {
            return vec3{};
        }

        const auto& material = m_materials[out.material];
        const auto normal = m_meshes[out.mesh].get_normals()[out.triangle];

        vec3 irradiance{};

        constexpr u16 maxBounces = 4;
        constexpr u16 numSamples = 4;
        constexpr f32 sampleWeight = 1.f / numSamples;

        if (bounces < maxBounces)
        {
            const auto scatterDirection = hemisphere_uniform_sample(state.m_rng, normal);
            const auto selfIntersectBias = scatterDirection * .001f;
            const auto position = ray.direction * out.distance + ray.origin + selfIntersectBias;

            for (u16 sample = 0; sample < numSamples; ++sample)
            {
                irradiance += max(0.f, dot(normal, scatterDirection)) *
                              compute_lighting_recursive({position, scatterDirection}, state, bounces + 1);
            }
        }

        return irradiance * sampleWeight * material.albedo + material.emissive;
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

    void raytracer_state::resize(u16 width, u16 height, u16 tileSize)
    {
        m_radianceBuffer.clear();
        m_accumulationBuffer.clear();

        const auto area = i32{width} * height;

        m_width = width;
        m_height = height;
        m_radianceBuffer.resize(area);

        m_tileSize = tileSize;
        m_numTilesX = round_up_div(width, tileSize);

        const auto numTiles = m_numTilesX * round_up_div(height, tileSize);
        m_accumulationBuffer.resize(numTiles);
    }

    void raytracer_state::reset_accumulation()
    {
        std::fill(m_radianceBuffer.begin(), m_radianceBuffer.end(), vec3{});
        std::fill(m_accumulationBuffer.begin(), m_accumulationBuffer.end(), 0u);
        m_metrics = {};
    }

    u32 raytracer_state::get_num_samples_at(u16 x, u16 y) const
    {
        const auto offset = get_accumulation_offset(x, y);
        return m_accumulationBuffer[offset];
    }

    u32 raytracer_state::get_accumulation_offset(u16 x, u16 y) const
    {
        const auto tileX = round_up_div(x, m_tileSize);
        const auto tileY = round_up_div(y, m_tileSize);
        return u32{m_numTilesX} * tileY + tileX;
    }
}