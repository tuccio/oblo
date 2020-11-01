#include <gtest/gtest.h>

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/math/triangle.hpp>

namespace oblo
{
    namespace
    {
        const triangle s_cube[] = { // front face
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}}},
            {{{.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {.5f, .5f, -.5f}}},
            // back face
            {{{-.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}}},
            {{{.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, -.5f, .5f}}},
            // left face
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {-.5f, .5f, .5f}}},
            {{{-.5f, -.5f, -.5f}, {-.5f, .5f, .5f}, {-.5f, -.5f, .5f}}},
            // right face
            {{{.5f, -.5f, -.5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}}},
            {{{.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}}}};
    }

    TEST(bvh, build)
    {

        triangle_container container;
        container.add(s_cube);

        bvh bvh;
        bvh.build(container);
    }
}