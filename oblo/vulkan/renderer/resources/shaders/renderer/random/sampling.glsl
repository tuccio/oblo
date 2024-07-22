#ifndef OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING
#define OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING

#include <renderer/constants>

/// Samples a disk of given radius.
/// \param radius The radius of the disk.
/// \param u Random value in [0, 1]
vec2 random_sample_uniform_disk(in float radius, in vec2 u)
{
    const float r = radius * u.x;
    const float phi = 2 * float_pi() * u.y;

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

    const vec3 v = N.z > -.9f && N.z < .9f ? vec3(0, 0, 1) : vec3(1, 0, 0);

    vec3 T = cross(N, v);
    const vec3 B = cross(T, N);
    T = cross(B, N);

    return T * disk.x + B * disk.y;
}

#endif