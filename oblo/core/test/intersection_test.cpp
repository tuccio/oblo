#include <gtest/gtest.h>

#include <oblo/math/frustum.hpp>
#include <oblo/math/frustum_intersection.hpp>
#include <oblo/math/ray_intersection.hpp>
#include <oblo/math/view_projection.hpp>

#include <iterator>
#include <random>

namespace oblo
{
    TEST(ray_intersection, ray_triangle_basic)
    {
        struct triangle_test
        {
            triangle triangle;
            ray ray;
            bool expectedIntersection;
            f32 expectedDistance;
        };

        triangle_test tests[] = {triangle_test{triangle{{{-1.f, -1.f, 1.f}, {0.f, 1.f, 1.f}, {1.f, -1.f, 1.f}}},
                                     ray{.origin = {}, .direction = {0.f, 0.f, 1.f}},
                                     true,
                                     1.f},
            triangle_test{triangle{{{-1.f, -1.f, 1.f}, {0.f, 1.f, 1.f}, {1.f, -1.f, 1.f}}},
                ray{.origin = {}, .direction = {0.f, 0.f, -1.f}},
                false,
                -1.f}};

        for (auto it = std::begin(tests); it != std::end(tests); ++it)
        {
            auto triangle = it->triangle;
            auto ray = it->ray;
            const auto expectedIntersection = it->expectedIntersection;
            const auto expectedDistance = it->expectedDistance;

            f32 d;
            ASSERT_EQ(expectedIntersection, intersect(ray, triangle, d));

            if (expectedIntersection)
            {
                ASSERT_NEAR(expectedDistance, d, .001f);
            }
        }
    }

    TEST(ray_intersection, ray_triangle_random)
    {
        constexpr auto NumTriangles = 32;
        constexpr auto NumTestsPerTriangle = 16;

        std::mt19937 rng{42};
        std::uniform_real_distribution<f32> vertexDist{-1.f, 1.f};

        for (int i = 0; i < NumTriangles;)
        {
            triangle triangle;

            triangle.v[0] = {vertexDist(rng), vertexDist(rng), vertexDist(rng)};
            triangle.v[1] = {vertexDist(rng), vertexDist(rng), vertexDist(rng)};
            triangle.v[2] = {vertexDist(rng), vertexDist(rng), vertexDist(rng)};

            const auto edge1 = triangle.v[1] - triangle.v[0];
            const auto edge2 = triangle.v[2] - triangle.v[0];

            const auto e1xe2 = cross(edge1, edge2);

            // Skip small/degenerate triangles
            if (length(e1xe2) < 0.01f)
            {
                continue;
            }

            for (int j = 0; j < NumTestsPerTriangle;)
            {
                std::uniform_real_distribution<f32> barycentricCoordsDist{0.f, 1.f};

                const auto r = barycentricCoordsDist(rng);
                const auto s = barycentricCoordsDist(rng);
                const auto t = 1 - (r + s);

                const vec3 point = triangle.v[0] * r + triangle.v[1] * s + triangle.v[2] * t;
                const vec3 origin = {vertexDist(rng), vertexDist(rng), vertexDist(rng)};
                const vec3 direction = point - origin;

                const bool isOnTriangle = t >= 0.f;
                const bool expectedHit = isOnTriangle;
                const bool expectedHitCull = isOnTriangle && dot(e1xe2, direction) < 0.f;

                const auto expectedDistance = length(direction);
                const ray ray{.origin = origin, .direction = normalize(direction)};

                f32 distance;
                const bool hit = intersect(ray, triangle, distance);

                ASSERT_EQ(expectedHit, hit);

                if (hit)
                {
                    ASSERT_NEAR(expectedDistance, distance, 0.001f);
                }

                f32 distanceCull;
                const bool hitCull = intersect_cull(ray, triangle, distanceCull);
                ASSERT_EQ(expectedHitCull, hitCull);

                if (hitCull)
                {
                    ASSERT_NEAR(expectedDistance, distanceCull, 0.001f);
                }

                ++j;
            }

            ++i;
        }
    }

    TEST(frustum_intersection, frustum_aabb_intersection)
    {
        const auto view = make_look_at(vec3{}, vec3{.y = 1.f}, vec3{.z = 1.f});
        const auto perspective = make_perspective_matrix(75_rad, 1.f, .001f, 100.f);

        const auto viewProjection = perspective * view;
        const auto inverseViewProjection = inverse(viewProjection);

        ASSERT_TRUE(inverseViewProjection);

        const auto frustum = normalize(make_frustum_from_inverse_view_projection(*inverseViewProjection));

        ASSERT_TRUE(intersects_or_contains(frustum,
            aabb{
                .min = {.x = -5.f, .y = -5.f, .z = 5.f},
                .max = {.x = 5.f, .y = 5.f, .z = 10.f},
            }));

        ASSERT_FALSE(intersects_or_contains(frustum,
            aabb{
                .min = {.x = -5.f, .y = -5.f, .z = -10.f},
                .max = {.x = 5.f, .y = 5.f, .z = -5.f},
            }));

        ASSERT_TRUE(intersects_or_contains(frustum,
            aabb{
                .min = {.x = -5.f, .y = -5.f, .z = 90.f},
                .max = {.x = 5.f, .y = 5.f, .z = 120.f},
            }));

        ASSERT_FALSE(intersects_or_contains(frustum,
            aabb{
                .min = {.x = -5.f, .y = -5.f, .z = 101.f},
                .max = {.x = 5.f, .y = 5.f, .z = 120.f},
            }));

        ASSERT_FALSE(intersects_or_contains(frustum,
            aabb{
                .min = {.x = -5.f, .y = 200.f, .z = 5.f},
                .max = {.x = 5.f, .y = 210.f, .z = 10.f},
            }));
    }
}