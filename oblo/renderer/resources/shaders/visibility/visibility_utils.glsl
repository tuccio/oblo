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

vec2 visibility_calculate_position_ndc_2d(in mat4 viewProjection, in vec3 prevPositionWS)
{
    const vec4 prevPositionCS = viewProjection * vec4(prevPositionWS, 1);
    return prevPositionCS.xy / prevPositionCS.w;
}

vec2 visibility_calculate_position_ndc_2d(in mat4 viewProjection, in barycentric_coords bc, in triangle prevTriangleWS)
{
    const vec3 prevPositionWS = barycentric_interpolate(bc, prevTriangleWS.v);
    return visibility_calculate_position_ndc_2d(viewProjection, prevPositionWS);
}

vec3 visibility_calculate_position_ndc_3d(in mat4 viewProjection, in vec3 prevPositionWS)
{
    const vec4 prevPositionCS = viewProjection * vec4(prevPositionWS, 1);
    return prevPositionCS.xyz / prevPositionCS.w;
}

bool visibility_calculate_position_and_barycentric_coords(in uvec2 screenPos,
    in uvec2 resolution,
    in camera_buffer camera,
    in triangle triangleWS,
    out vec3 positionWS,
    out barycentric_coords bc,
    out barycentric_coords bcDDX,
    out barycentric_coords bcDDY)
{
    // Cast a ray from the camera to the near plane and calculate the distance of the ray hit to the plane on the
    // triangle in world space, we use that to derive the position in world space
    const vec2 ndc = screen_to_ndc(screenPos, resolution);
    const ray cameraRay = visibility_calculate_camera_ray(camera, ndc);

    float intersectionDistance;

    // Really the plan should be hitting here, since we already know the triangle was rendered by the rasterizer, we
    // mostly want to calculate at what distance it does
    if (!distance_from_triangle_plane(cameraRay, triangleWS, intersectionDistance))
    {
        return false;
    }

    ray cameraRayDDX;
    cameraRayDDX.origin = camera.position;
    cameraRayDDX.direction = subgroupQuadSwapHorizontal(cameraRay.direction);

    float intersectionDistanceDDX;
    distance_from_triangle_plane(cameraRayDDX, triangleWS, intersectionDistanceDDX);

    ray cameraRayDDY;
    cameraRayDDY.origin = camera.position;
    cameraRayDDY.direction = subgroupQuadSwapVertical(cameraRay.direction);

    // We do the same ray tracing with the nearby quads, so we can calculate UV gradients for our sampler
    float intersectionDistanceDDY;
    distance_from_triangle_plane(cameraRayDDX, triangleWS, intersectionDistanceDDY);

    positionWS = ray_point_at(cameraRay, intersectionDistance);
    const vec3 positionDDX = ray_point_at(cameraRayDDX, intersectionDistanceDDX);
    const vec3 positionDDY = ray_point_at(cameraRayDDY, intersectionDistanceDDY);

    barycentric_calculate(bc, triangleWS.v, positionWS);
    barycentric_calculate(bcDDX, triangleWS.v, positionDDX);
    barycentric_calculate(bcDDX, triangleWS.v, positionDDY);

    return true;
}

#endif