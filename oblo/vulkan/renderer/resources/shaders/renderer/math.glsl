#ifndef OBLO_INCLUDE_RENDERER_MATH
#define OBLO_INCLUDE_RENDERER_MATH

float pow2(in float x)
{
    return x * x;
}

float saturate(in float value)
{
    return max(0.f, value);
}

#endif