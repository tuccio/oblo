#include <gtest/gtest.h>

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/math/triangle.hpp>

namespace oblo
{
    namespace
    {
        constexpr triangle s_cube[] = {
            // front face
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}}},
            {{{.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {.5f, .5f, -.5f}}},
            // back face
            {{{-.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}}},
            {{{.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, -.5f, .5f}}},
            // left face
            {{{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}}},
            {{{.5f, -.5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}}},
            // right face
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}}},
            {{{-.5f, -.5f, -.5f}, {-.5f, -.5f, .5f}, {-.5f, .5f, .5f}}},
            // top face
            {{{-.5f, .5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, -.5f}}},
            {{{.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}}},
            // bottom face
            {{{-.5f, -.5f, .5f}, {-.5f, -.5f, -.5f}, {.5f, -.5f, .5f}}},
            {{{.5f, -.5f, .5f}, {-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}}},
        };
    }

    TEST(triangle_container, cube_bounds)
    {
        triangle_container container;
        container.add(s_cube);

        constexpr auto numTriangles = std::size(s_cube);
        ASSERT_EQ(container.size(), numTriangles);

        constexpr auto expectedBounds = aabb{.min = {-.5f, -.5f, -.5f}, .max = {.5f, .5f, .5f}};
        const auto bounds = container.primitives_bounds(0, numTriangles);

        ASSERT_EQ(bounds.min, expectedBounds.min);
        ASSERT_EQ(bounds.max, expectedBounds.max);
    }

    TEST(triangle_container, partition)
    {
        triangle_container container;
        container.add(s_cube);

        constexpr auto numTriangles = std::size(s_cube);
        ASSERT_EQ(container.size(), numTriangles);

        for (u8 axis = 0; axis < 3; ++axis)
        {
            {
                const auto midPoint = container.partition_by_axis(0, numTriangles, axis, -.51f);
                ASSERT_EQ(midPoint, 0);
            }

            {
                const auto midPoint = container.partition_by_axis(0, numTriangles, axis, .51f);
                ASSERT_EQ(midPoint, numTriangles);
            }
        }
    }

    TEST(bvh, build_cube)
    {
        triangle_container container;
        container.add(s_cube);

        bvh bvh;
        bvh.build(container);

        u32 numTotalTriangles = 0;

        bvh.visit([&numTotalTriangles](u32, aabb, u32, u32 numPrimitives) { numTotalTriangles += numPrimitives; });

        ASSERT_EQ(std::size(s_cube), numTotalTriangles);
    }
}