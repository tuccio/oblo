#include <renderer/debug/printf>
#include <renderer/quad>


#define BLUR_KERNEL_END (BLUR_KERNEL_SIZE >> 1)
#define BLUR_KERNEL_BEGIN (-BLUR_KERNEL_END)

layout(binding = 0, BLUR_IMAGE_FORMAT) uniform restrict readonly image2D t_InSource;
layout(binding = 1, BLUR_IMAGE_FORMAT) uniform restrict writeonly image2D t_OutBlurred;

layout(push_constant) uniform c_PushConstants
{
    uvec2 resolution;
}
g_Constants;

ivec2 blur_pixel_offset_clamp(in uvec2 pixel, in int offset)
{
#ifdef BLUR_HORIZONTAL
    return ivec2(clamp(pixel.x + offset, 0, g_Constants.resolution.x), pixel.y);
#else
    return ivec2(pixel.x, clamp(pixel.y + offset, 0, g_Constants.resolution.y));
#endif
}

const float g_Kernel[BLUR_KERNEL_SIZE] = {BLUR_KERNEL_DATA};

float blur_get_kernel(in int offset)
{
    return g_Kernel[offset];
}

vec4 blur_image_load(in ivec2 position)
{
    return imageLoad(t_InSource, position);
}

vec4 blur_image_load_offset(in uvec2 base, in int offset)
{
    return blur_image_load(blur_pixel_offset_clamp(base, offset));
}

vec4 blur_pixel(in uvec2 pixel);

void main()
{
    // const uvec2 localGroupId = quad_remap_lane_8x8(gl_LocalInvocationIndex);
    // const uvec2 screenPos = gl_WorkGroupID.xy * 8 + localGroupId;

    const uvec2 screenPos = gl_GlobalInvocationID.xy;

    if (screenPos.x >= g_Constants.resolution.x)
    {
        return;
    }

#if 1
    const vec4 v = blur_pixel(screenPos);
#else
    const vec4 v = imageLoad(t_InSource, ivec2(screenPos));
#endif

    imageStore(t_OutBlurred, ivec2(screenPos), v);
}