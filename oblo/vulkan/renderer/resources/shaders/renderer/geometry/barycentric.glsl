#ifndef OBLO_INCLUDE_RENDERER_BARYCENTRIC
#define OBLO_INCLUDE_RENDERER_BARYCENTRIC

struct barycentric_coords
{
    vec3 lambda;
};

void barycentric_calculate(out barycentric_coords coords, in vec3 triangle[3], in vec3 p)
{
    const vec3 e0 = triangle[1] - triangle[0];
    const vec3 e1 = triangle[2] - triangle[0];
    const vec3 e2 = p - triangle[0];

    const float d00 = dot(e0, e0);
    const float d01 = dot(e0, e1);
    const float d11 = dot(e1, e1);
    const float d20 = dot(e2, e0);
    const float d21 = dot(e2, e1);

    const float denom = d00 * d11 - d01 * d01;

    const float v = (d11 * d20 - d01 * d21) / denom;
    const float w = (d00 * d21 - d01 * d20) / denom;
    const float u = 1.f - v - w;

    coords.lambda = vec3(u, v, w);
}

float barycentric_interpolate(in barycentric_coords coords, float a, float b, float c)
{
    return dot(vec3(a, b, c), coords.lambda);
}

vec2 barycentric_interpolate(in barycentric_coords coords, in vec2 values[3])
{
    return coords.lambda[0] * values[0] + coords.lambda[1] * values[1] + coords.lambda[2] * values[2];
}

vec3 barycentric_interpolate(in barycentric_coords coords, in vec3 values[3])
{
    return coords.lambda[0] * values[0] + coords.lambda[1] * values[1] + coords.lambda[2] * values[2];
}

void barycentric_partial_derivatives(in barycentric_coords bcDDX,
    in barycentric_coords bcDDY,
    in vec2 v,
    in vec2 triangle[3],
    out vec2 ddx,
    out vec2 ddy)
{
    const vec2 quadX = barycentric_interpolate(bcDDX, triangle);
    const vec2 quadY = barycentric_interpolate(bcDDY, triangle);

    ddx = v - quadX;
    ddy = v - quadY;
}

void barycentric_partial_derivatives(in barycentric_coords bcDDX,
    in barycentric_coords bcDDY,
    in vec3 v,
    in vec3 triangle[3],
    out vec3 ddx,
    out vec3 ddy)
{
    const vec3 quadX = barycentric_interpolate(bcDDX, triangle);
    const vec3 quadY = barycentric_interpolate(bcDDY, triangle);

    ddx = v - quadX;
    ddy = v - quadY;
}

#endif
