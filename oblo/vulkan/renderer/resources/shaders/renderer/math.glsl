#ifndef OBLO_INCLUDE_RENDERER_MATH
#define OBLO_INCLUDE_RENDERER_MATH

float pow2(in float x)
{
    return x * x;
}

float pow5(in float x)
{
    return pow2(pow2(x)) * x;
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

vec3 pick_orthogonal(in vec3 N)
{
    const vec3 v = N.z > -.99f && N.z < .99f ? vec3(0, 0, 1) : vec3(1, 0, 0);
    return cross(N, v);
}

#endif