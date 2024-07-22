#ifndef OBLO_INCLUDE_RENDERER_RANDOM_RANDOM
#define OBLO_INCLUDE_RENDERER_RANDOM_RANDOM

/// Implementation of Tiny Encryption Algorithm.
uint hash_tea(in uint val0, in uint val1)
{
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for (uint n = 0; n < 16; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

/// Generates a seed based on 2 independent integer values.
uint random_seed(in uint u0, in uint u1)
{
    return hash_tea(u0, u1);
}

/// Implementation of PCG, takes a seed and updates it.
/// \see https://www.pcg-random.org/
uint hash_pcg(inout uint state)
{
    uint prev = state * 747796405u + 2891336453u;
    uint word = ((prev >> ((prev >> 28u) + 4u)) ^ prev) * 277803737u;
    state = prev;
    return (word >> 22u) ^ word;
}

/// Generates a random float in [0, 1).
float random_uniform_1d(inout uint seed)
{
    uint r = hash_pcg(seed);
    return uintBitsToFloat(0x3f800000 | (r >> 9)) - 1.0f;
}

/// Generates a random vec2 in [0, 1).
vec2 random_uniform_2d(inout uint seed)
{
    vec2 r;
    r.x = random_uniform_1d(seed);
    r.y = random_uniform_1d(seed);
    return r;
}

#endif