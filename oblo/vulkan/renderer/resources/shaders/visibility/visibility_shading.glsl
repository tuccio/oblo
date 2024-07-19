#include <renderer/geometry/barycentric>
#include <renderer/geometry/ray>
#include <renderer/quad>
#include <visibility/visibility_buffer>

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 16) uniform b_CameraBuffer
{
    camera_buffer g_Camera;
};

layout(binding = 11, rg32ui) uniform restrict readonly uimage2D t_InVisibilityBuffer;

layout(binding = 12, rgba8) uniform restrict writeonly image2D t_OutShadedImage;

layout(push_constant) uniform c_PushConstants
{
    uvec2 resolution;
}
g_Constants;

vec4 visibility_shade(in uvec2 screenPos, in visibility_buffer_data vb);

void main()
{
    const uvec2 localGroupId = quad_remap_lane_8x8(gl_LocalInvocationIndex);
    const ivec2 screenPos = ivec2(gl_WorkGroupID.xy * 8 + localGroupId);

    if (screenPos.x >= g_Constants.resolution.x)
    {
        return;
    }

    // Parse the visibility buffer to find which triangle we are processing
    const uvec4 visBufferData = imageLoad(t_InVisibilityBuffer, screenPos);

    visibility_buffer_data vb;

    if (!visibility_buffer_parse(visBufferData.xy, vb))
    {
        // This means we didn't hit anything, and have no triangle in this pixel
        imageStore(t_OutShadedImage, screenPos, vec4(0));
        return;
    }

    const vec4 color = visibility_shade(uvec2(screenPos), vb);
    imageStore(t_OutShadedImage, screenPos, color);
}

bool calculate_position_and_barycentric_coords(in uvec2 screenPos,
    in triangle triangleWS,
    out vec3 positionWS,
    out barycentric_coords bc,
    out barycentric_coords bcDDX,
    out barycentric_coords bcDDY)
{
    // Cast a ray from the camera to the near plane and calculate the distance of the ray hit to the plane on the
    // triangle in world space, we use that to derive the position in world space
    ray cameraRay;
    cameraRay.origin = g_Camera.position;
    const vec2 positionNDC = screen_to_ndc(screenPos, g_Constants.resolution);
    cameraRay.direction = camera_ray_direction(g_Camera, positionNDC);

    float intersectionDistance;

    // Really the plan should be hitting here, since we already know the triangle was rendered by the rasterizer, we
    // mostly want to calculate at what distance it does
    if (!distance_from_triangle_plane(cameraRay, triangleWS, intersectionDistance))
    {
        return false;
    }

    ray cameraRayDDX;
    cameraRayDDX.origin = g_Camera.position;
    cameraRayDDX.direction = subgroupQuadSwapHorizontal(cameraRay.direction);

    float intersectionDistanceDDX;
    distance_from_triangle_plane(cameraRayDDX, triangleWS, intersectionDistanceDDX);

    ray cameraRayDDY;
    cameraRayDDY.origin = g_Camera.position;
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