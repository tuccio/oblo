#ifndef OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING
#define OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING

#include <renderer/constants>

/// Samples a disk of given radius.
/// \param radius The radius of the disk.
/// \param u Random value in [0, 1]
vec2 random_sample_uniform_disk(in float radius, in vec2 u)
{
    const float r = radius * sqrt(u[0]);
    const float phi = 2 * float_pi() * u[1];

    vec2 p;
    p.x = r * cos(phi);
    p.y = r * sin(phi);

    return p;
}

/// Samples a disk of given radius oriented with the given normal.
/// \param radius The radius of the disk.
/// \param N The normal to orientate the disk towards.
/// \param u Random value in [0, 1]
vec3 random_sample_uniform_disk(in float radius, in vec3 N, in vec2 u)
{
    const vec2 disk = random_sample_uniform_disk(radius, u);

    vec3 T = cross(N, new_orthogonal(N));
    const vec3 B = cross(T, N);
    T = cross(B, N);

    return T * disk.x + B * disk.y;
}

/// Samples a uniform direction in a cone oriented towards +Z.
vec3 random_sample_uniform_cone(in float cosThetaMax, in vec2 u)
{
    const float cosTheta = (1 - u[0]) + u[0] * cosThetaMax;
    const float sinTheta = sqrt(1 - cosTheta * cosTheta);
    const float phi = u[1] * 2 * float_pi();

    vec3 r;
    r.x = cos(phi) * sinTheta;
    r.y = sin(phi) * sinTheta;
    r.z = cosTheta;

    return r;
}

#endif