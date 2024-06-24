#ifndef OBLO_INCLUDE_RENDERER_BARYCENTRIC
#define OBLO_INCLUDE_RENDERER_BARYCENTRIC

struct barycentric_coords_deriv
{
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

void barycentric_calculate(out barycentric_coords_deriv coords, in vec3 triangle[3], in vec3 p)
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
    const float u = 1.0 - v - w;

    coords.lambda = vec3(u, v, w);

    // const float invDet = rcp(determinant(mat2(triangle[2] - triangle[1], triang - ndc1)));

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

float barycentric_interpolate(in barycentric_coords_deriv coords, float a, float b, float c)
{
    return dot(vec3(a, b, c), coords.lambda);
}

vec2 barycentric_interpolate(in barycentric_coords_deriv coords, in vec2 values[3])
{
    return vec2(barycentric_interpolate(coords, values[0].x, values[1].x, values[2].x),
        barycentric_interpolate(coords, values[0].y, values[1].y, values[2].y));
}

vec3 barycentric_interpolate(in barycentric_coords_deriv coords, in vec3 values[3])
{
    return vec3(barycentric_interpolate(coords, values[0].x, values[1].x, values[2].x),
        barycentric_interpolate(coords, values[0].y, values[1].y, values[2].y),
        barycentric_interpolate(coords, values[0].z, values[1].z, values[2].z));
}

#endif
