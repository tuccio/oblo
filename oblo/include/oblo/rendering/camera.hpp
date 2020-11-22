#pragma once

#include <oblo/math/angle.hpp>
#include <oblo/math/ray.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>

namespace oblo
{
    struct camera
    {
        vec3 position;
        vec3 left;
        vec3 up;
        vec3 forward;
        radians fovx;
        radians fovy;
        f32 tanHalfFovX;
        f32 tanHalfFovY;
        f32 near;
        f32 far;
    };

    inline ray ray_cast(const camera& camera, const vec2& uv)
    {
        const auto nearCenter = camera.forward * camera.near;
        const auto nearPosition =
            nearCenter - uv.x * camera.tanHalfFovX * camera.left - uv.y * camera.tanHalfFovY * camera.up;

        return ray{camera.position, normalize(nearPosition - camera.position)};
    }

    inline void camera_set_look_at(camera& camera, const vec3& position, const vec3& target, const vec3& up)
    {
        camera.position = position;
        camera.up = normalize(up);
        camera.forward = normalize(target - position);
        camera.left = normalize(cross(camera.up, camera.forward));
    }

    inline void camera_set_vertical_fov(camera& camera, radians fov)
    {
        const auto yfov = fov / 2.f;
        camera.tanHalfFovY = std::tan(f32{yfov});
        camera.fovy = fov;
    }

    inline void camera_set_horizontal_fov(camera& camera, radians fov)
    {
        const auto xfov = fov / 2.f;
        camera.tanHalfFovX = std::tan(f32{xfov});
        camera.fovx = fov;
    }
}