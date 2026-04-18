#ifndef OBLO_INCLUDE_RENDERER_SKYBOX_SKYBOX
#define OBLO_INCLUDE_RENDERER_SKYBOX_SKYBOX

#include <renderer/constants>
#include <renderer/geometry/ray>
#include <skybox/skybox_utility>

vec4 skybox_sample_screenspace(
    in uint skyboxTexture, in vec3 skyboxMultiplier, in ray cameraRay, in uvec2 screenPos, in uvec2 resolution)
{
    // Generate sphere UVs to sample the skybox
    const vec2 uv = skybox_uv_from_ray_direction(cameraRay.direction);

    const vec2 uvQuadX = subgroupQuadSwapHorizontal(uv);
    const vec2 uvQuadY = subgroupQuadSwapVertical(uv);

#if 0 // Using gradients this way introduces a seam where the skybox wraps, maybe we can just choose a mipmap
      // manually instead
    const vec2 uvDDX = uv - uvQuadX;
    const vec2 uvDDY = uv - uvQuadY;

    const vec4 color = texture_sample_2d_grad(skyboxTexture, OBLO_SAMPLER_LINEAR_REPEAT, uv, uvDDX, uvDDY);

#else
    const uint lod = 0;
    const vec4 color = texture_sample_2d_lod(skyboxTexture, OBLO_SAMPLER_LINEAR_REPEAT, uv, 0);
#endif

    return vec4(color.xyz * skyboxMultiplier, 1);
}

#endif