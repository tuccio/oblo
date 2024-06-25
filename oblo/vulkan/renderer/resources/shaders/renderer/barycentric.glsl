#ifndef OBLO_INCLUDE_RENDERER_BARYCENTRIC
#define OBLO_INCLUDE_RENDERER_BARYCENTRIC

struct barycentric_coords
{
    vec3 lambda;
};

void barycentric_calculate(out barycentric_coords coords, in vec3 triangle[3], in vec3 p)
{
    // vec3 a = triangle[0];
    // vec3 b = triangle[1];
    // vec3 c = triangle[2];

    // vec3 v0 = b - a;
    // vec3 v1 = c - a;
    // vec3 v2 = p - a;
    // float d00 = dot(v0, v0);
    // float d01 = dot(v0, v1);
    // float d11 = dot(v1, v1);
    // float d20 = dot(v2, v0);
    // float d21 = dot(v2, v1);
    // float denom = d00 * d11 - d01 * d01;
    // float v = (d11 * d20 - d01 * d21) / denom;
    // float w = (d00 * d21 - d01 * d20) / denom;
    // float u = 1.0 - v - w;
    // coords.lambda = vec3(u, v, w);

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

    // const vec3 e0 = triangle[1] - triangle[0];
    // const vec3 e1 = triangle[2] - triangle[1];
    // const vec3 e2 = p - triangle[0];

    // const float d00 = dot(e0, e0);
    // const float d01 = dot(e0, e1);
    // const float d11 = dot(e1, e1);
    // const float d20 = dot(e2, e0);
    // const float d21 = dot(e2, e1);
    // const float denom = d00 * d11 - d01 * d01;

    // coords.lambda.y = (d11 * d20 - d01 * d21) / denom;
    // coords.lambda.z = (d00 * d21 - d01 * d20) / denom;
    // coords.lambda.x = 1.f - coords.lambda.y - coords.lambda.z;
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

#endif
