#ifndef OBLO_INCLUDE_VISIBILITY_VISIBILITY_UTILS
#define OBLO_INCLUDE_VISIBILITY_VISIBILITY_UTILS

#include <renderer/camera>
#include <renderer/geometry/ray>

ray visibility_calculate_camera_ray(in camera_buffer camera, in uvec2 screenPos, in uvec2 resolution)
{
    // Cast a ray from the camera to the near plane and calculate the distance of the ray hit to the plane on the
    // triangle in world space, we use that to derive the position in world space
    ray cameraRay;
    cameraRay.origin = camera.position;
    const vec2 positionNDC = screen_to_ndc(screenPos, resolution);
    cameraRay.direction = camera_ray_direction(camera, positionNDC);

    return cameraRay;
}

vec3 visibility_calculate_position(in ray cameraRay, in triangle triangleWS)
{
    float intersectionDistance;
    distance_from_triangle_plane(cameraRay, triangleWS, intersectionDistance);

    return ray_point_at(cameraRay, intersectionDistance);
}

#endif