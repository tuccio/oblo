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

vec2 saturate(in vec2 value)
{
    return max(vec2(0.f), value);
}

vec3 saturate(in vec3 value)
{
    return max(vec3(0.f), value);
}

uint round_up_div(in uint numerator, in uint denominator)
{
    return (numerator + denominator - 1) / denominator;
}

#endif