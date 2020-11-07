#pragma once

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/rendering/camera.hpp>


namespace oblo
{
    class debug_renderer;

    struct sandbox_state
    {
        debug_renderer* debugRenderer{nullptr};

        camera camera;

        triangle_container triangles;
        bvh bvh;

        bool renderRasterized{false};
    };
}