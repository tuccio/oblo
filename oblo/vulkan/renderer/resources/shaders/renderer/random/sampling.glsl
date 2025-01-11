#ifndef OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING
#define OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING

#include <renderer/constants>
#include <renderer/math>

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
/// \param u Random values in [0, 1]
vec3 random_sample_uniform_disk(in float radius, in vec3 N, in vec2 u)
{
    const vec2 disk = random_sample_uniform_disk(radius, u);

    vec3 T = cross(N, pick_orthogonal(N));
    const vec3 B = cross(T, N);
    T = cross(B, N);

    return T * disk.x + B * disk.y;
}

/// Samples a hemisphere of given radius oriented with the given normal.
/// \param N The normal to orientate the hemisphere towards.
/// \param u Random values in [0, 1]
vec3 random_sample_uniform_hemisphere(in vec3 N, in vec2 u)
{
    const float r = sqrt(1.0 - u.x * u.x);
    const float phi = 2.f * float_pi() * u.y;

    vec3 T = cross(N, pick_orthogonal(N));
    const vec3 B = cross(T, N);
    T = cross(B, N);

    return normalize(r * sin(phi) * B + u.x * N + r * cos(phi) * T);
}

#endif