#pragma once

#include <oblo/acceleration/bvh.hpp>
#include <oblo/acceleration/triangle_container.hpp>
#include <oblo/raytracer/camera.hpp>

#include <string>

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
        bool autoImportLastScene{false};
        bool writeConfigOnShutdown{true};

        bool movedCamera{false};

        std::string latestImportedScene;
    };
}