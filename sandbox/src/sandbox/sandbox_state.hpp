#pragma once

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/rendering/camera.hpp>

namespace oblo
{
    class debug_renderer;
    class raytracer;
    class raytracer_state;

    struct sandbox_state
    {
        raytracer* raytracer;
        raytracer_state* raytracerState{nullptr};

        debug_renderer* debugRenderer{nullptr};

        camera camera;

        bool renderRasterized{false};
    };
}