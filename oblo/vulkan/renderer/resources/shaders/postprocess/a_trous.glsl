#ifndef OBLO_INCLUDE_POSTPROCESS_A_TROUS
#define OBLO_INCLUDE_POSTPROCESS_A_TROUS

const uint g_ATrousPassIndex = A_TROUS_PASS_INDEX;
const uint g_ATrousStride = 1u << g_ATrousPassIndex;

// We run 8x8 groups
const uint g_ATrousThreadLocalSize = 8;

// We only tap 3 pixels: the center, 1 at the left and 1 at the right, but each pass has different stride
const uint g_ATrousTapsPerPixel = 3;
const uint g_ATrousTapRadius = g_ATrousTapsPerPixel / 2;

const uint g_ATrousPadding = g_ATrousTapRadius * g_ATrousStride;
const uint g_ATrousCacheSize = g_ATrousThreadLocalSize + 2 * g_ATrousPadding;
const uint g_ATrousLoadsPerThread = (g_ATrousCacheSize + g_ATrousThreadLocalSize - 1) / g_ATrousThreadLocalSize;

layout(local_size_x = g_ATrousThreadLocalSize, local_size_y = g_ATrousThreadLocalSize, local_size_z = 1) in;

/// The function the user needs to implement, loading into shared cache.
void a_trous_load_pixel(in ivec2 pixelCoords, in uvec2 cacheOffset, in uvec2 resolution);

ivec2 a_trous_apply_offset_clamp(in ivec2 pixel, in uvec2 offset, in uvec2 resolution)
{
    return ivec2(max(0, min(int(resolution.x), int(pixel.x + offset.x))), max(0, min(int(resolution.y), int(pixel.y + offset.y))));
}

void a_trous_load_shared_cache(in uvec2 resolution)
{
    const ivec2 firstGroupPixel = ivec2(gl_WorkGroupID.xy * gl_WorkGroupSize.xy);
    const ivec2 firstPixelToLoad = firstGroupPixel - ivec2(g_ATrousPadding);

    [[unroll]] for (uint loadY = 0; loadY < g_ATrousLoadsPerThread; ++loadY)
    {
        [[unroll]] for (uint loadX = 0; loadX < g_ATrousLoadsPerThread; ++loadX)
        {
            const uvec2 offset = gl_WorkGroupSize.xy * uvec2(loadX, loadY) + gl_LocalInvocationID.xy;

            if (offset.x < g_ATrousCacheSize && offset.y < g_ATrousCacheSize)
            {
                const ivec2 coords = a_trous_apply_offset_clamp(firstPixelToLoad, offset, resolution);
                a_trous_load_pixel(coords, offset, resolution);
            }
        }
    }
}

#endif