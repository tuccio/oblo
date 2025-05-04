#include <renderer/quad>

#if BLUR_IMAGE_CHANNELS == 1
    #define blur_pixel_t float

#elif BLUR_IMAGE_CHANNELS == 2
    #define blur_pixel_t vec2
#else
    #error "Unsupported channels count"
#endif

const uint g_GroupSize = BLUR_GROUP_SIZE;
const uint g_KernelDataSize = BLUR_KERNEL_DATA_SIZE;
const uint g_ImageCacheSize = g_GroupSize + 2 * g_KernelDataSize;

const uint g_LoadsPerThread = (g_ImageCacheSize + g_GroupSize - 1) / g_GroupSize;

const float g_Kernel[g_KernelDataSize] = {BLUR_KERNEL_DATA};

shared blur_pixel_t g_ImageCache[g_ImageCacheSize];

#ifdef BLUR_HORIZONTAL
layout(local_size_x = BLUR_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
#else
layout(local_size_x = 1, local_size_y = BLUR_GROUP_SIZE, local_size_z = 1) in;
#endif

layout(binding = 0, BLUR_IMAGE_FORMAT) uniform restrict readonly image2D t_InSource;
layout(binding = 1, BLUR_IMAGE_FORMAT) uniform restrict writeonly image2D t_OutBlurred;

struct blur_context
{
    /// @brief The index of the pixel being processed in the shared cache array.
    int pixelCacheIndex;
};

/// @brief This function has to be implemented by the user.
blur_pixel_t blur_execute(in blur_context ctx);

blur_pixel_t blur_make_pixel(in vec4 v);
vec4 blur_make_vec4(in blur_pixel_t v);

float blur_get_kernel(in uint offset)
{
    return g_Kernel[offset];
}

blur_pixel_t blur_read_pixel_at_offset(in blur_context ctx, int offset)
{
    const int index = ctx.pixelCacheIndex + offset;
    return g_ImageCache[index];
}

ivec2 blur_apply_offset_clamp_edge(in ivec2 pixel, in uint offset, in ivec2 resolution)
{
#if defined(BLUR_HORIZONTAL)
    return ivec2(max(0, min(resolution.x - 1, pixel.x + offset)), pixel.y);
#elif defined(BLUR_VERTICAL)
    return ivec2(pixel.x, max(0, min(resolution.y - 1, pixel.y + offset)));
#endif
}

void main()
{
    const uvec2 resolution = imageSize(t_InSource);

#if defined(BLUR_HORIZONTAL)
    const ivec2 firstGroupPixel = ivec2(gl_WorkGroupID.x * g_GroupSize, gl_WorkGroupID.y);
    const ivec2 firstPixelToLoad = ivec2(firstGroupPixel.x - g_KernelDataSize, firstGroupPixel.y);
#elif defined(BLUR_VERTICAL)
    const ivec2 firstGroupPixel = ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y * g_GroupSize);
    const ivec2 firstPixelToLoad = ivec2(firstGroupPixel.x, firstGroupPixel.y - g_KernelDataSize);
#endif

    [[unroll]] for (uint loadIndex = 0; loadIndex < g_LoadsPerThread; ++loadIndex)
    {
        const uint offset = g_GroupSize * loadIndex + gl_LocalInvocationIndex;

        if (offset < g_ImageCacheSize)
        {
            const ivec2 coords = blur_apply_offset_clamp_edge(firstPixelToLoad, offset, ivec2(resolution));
            const vec4 pixel = imageLoad(t_InSource, coords);

            g_ImageCache[offset] = blur_make_pixel(pixel);
        }
    }

    memoryBarrierShared();
    barrier();

    const uvec2 screenPos = gl_GlobalInvocationID.xy;

    if (screenPos.x >= resolution.x || screenPos.y >= resolution.y)
    {
        return;
    }

    blur_context ctx;
    ctx.pixelCacheIndex = int(g_KernelDataSize + gl_LocalInvocationIndex);

    const blur_pixel_t v = blur_execute(ctx);

    imageStore(t_OutBlurred, ivec2(screenPos), blur_make_vec4(v));
}

#if BLUR_IMAGE_CHANNELS == 1
blur_pixel_t blur_make_pixel(in vec4 v)
{
    return v.x;
}

vec4 blur_make_vec4(in blur_pixel_t v)
{
    return vec4(v, 0, 0, 1);
}
#elif BLUR_IMAGE_CHANNELS == 2
    #define blur_pixel_t vec2

blur_pixel_t blur_make_pixel(in vec4 v)
{
    return v.xy;
}

vec4 blur_make_vec4(in blur_pixel_t v)
{
    return vec4(v.x, v.y, 0, 1);
}
#else
    #error "Unsupported channels count"
#endif