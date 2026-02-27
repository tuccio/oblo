#ifndef OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING
#define OBLO_INCLUDE_RENDERER_RANDOM_SAMPLING

#include <renderer/constants>
#include <renderer/math>

/// Samples a disk of given radius.
/// \param radius The radius of the disk.
/// \param u Random value in [0, 1]
vec2 random_sample_uniform_disk(in float radius, in vec2 u)
{
    // Concentring sampling method from PBR book 13.6.2
    const vec2 offset = 2.f * u - vec2(1.f);

    // Handle degeneracy at the origin
    if (offset.x == 0 && offset.y == 0)
    {
        return vec2(0.f);
    }

    float theta, r;

    if (abs(offset.x) > abs(offset.y))
    {
        r = offset.x;
        theta = .25f * float_pi() * (offset.y / offset.x);
    }
    else
    {
        r = offset.y;
        theta = .5f * float_pi() - .25f * float_pi() * (offset.x / offset.y);
    }

    return r * radius * vec2(cos(theta), sin(theta));
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

/// Uniformly samples a hemisphere of given radius oriented with the given normal.
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

/// Samples a cosine-weighted hemisphere of given radius oriented with the given normal.
/// \param N The normal to orientate the hemisphere towards.
/// \param u Random values in [0, 1].
vec3 random_sample_cosine_hemisphere(in vec3 N, in vec2 u)
{
    const vec2 d = random_sample_uniform_disk(1.f, u);

    const float z = sqrt(max(0.f, 1 - d.x * d.x - d.y * d.y));

    vec3 T = cross(N, pick_orthogonal(N));
    const vec3 B = cross(T, N);
    T = cross(B, N);

    return mat3(T, B, N) * vec3(d.x, d.y, z);
}

#endif