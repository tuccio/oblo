#ifndef OBLO_INCLUDE_VISIBILITY_VISIBILITY_UTILS
#define OBLO_INCLUDE_VISIBILITY_VISIBILITY_UTILS

#include <renderer/camera>
#include <renderer/geometry/barycentric>
#include <renderer/geometry/ray>

ray visibility_calculate_camera_ray(in camera_buffer camera, in vec2 positionNDC)
{
    // Cast a ray from the camera to the near plane and calculate the distance of the ray hit to the plane on the
    // triangle in world space, we use that to derive the position in world space
    ray cameraRay;
    cameraRay.origin = camera.position;
    cameraRay.direction = camera_ray_direction(camera, positionNDC);

    return cameraRay;
}

vec3 visibility_calculate_position(in ray cameraRay, in triangle triangleWS)
{
    float intersectionDistance;
    distance_from_triangle_plane(cameraRay, triangleWS, intersectionDistance);

    return ray_point_at(cameraRay, intersectionDistance);
}

vec2 visibility_calculate_last_frame_position_ndc_2d(
    in vec2 ndc, in mat4 lastFrameViewProjection, in barycentric_coords bc, in triangle prevTriangleWS)
{
    const vec3 prevPositionWS = barycentric_interpolate(bc, prevTriangleWS.v);
    const vec4 prevPositionCS = lastFrameViewProjection * vec4(prevPositionWS, 1);

    return prevPositionCS.xy / prevPositionCS.w;
}

#endif