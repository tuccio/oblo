#ifndef OBLO_INCLUDE_RENDERER_MATH_SPHERICAL_HARMONICS
#define OBLO_INCLUDE_RENDERER_MATH_SPHERICAL_HARMONICS

struct sh2
{
    vec4 coefficients;
};

struct sh3
{
    float coefficients[9];
};

sh2 sh_add(in sh2 a, in sh2 b)
{
    sh2 r;
    r.coefficients = a.coefficients + b.coefficients;
    return r;
}

sh3 sh_add(in sh3 a, in sh3 b)
{
    sh3 r;

    [[unroll]] for (uint i = 0; i < 9; ++i)
    {
        r.coefficients[i] = a.coefficients[i] + b.coefficients[i];
    }

    return r;
}

float sh_dot(in sh2 a, in sh2 b)
{
    return dot(a.coefficients, b.coefficients);
}

float sh_dot(in sh3 a, in sh3 b)
{
    float r = a.coefficients[0] * b.coefficients[0];

    [[unroll]] for (uint i = 1; i < 9; ++i)
    {
        r += a.coefficients[i] * b.coefficients[i];
    }

    return r;
}

sh2 sh_mul(in sh2 a, in float v)
{
    sh3 r;

    [[unroll]] for (uint i = 0; i < 9; ++i)
    {
        r.coefficients[i] = a.coefficients[i] * v;
    }

    return r;
}

sh2 sh_eval2(in vec3 direction)
{
    // Evaluation of coefficients of polynomial form of SH basis, c.f. Stupid Spherical Harmonics (SH) Tricks Appendix 2
    const float l0m0 = 0.282094792f; // l = 0, m = 0: 1 / (2 * sqrt(pi))

    const float l1mn1 = -0.488602512f; // l = 1, m = -1 : -sqrt(3) / (2 * sqrt(pi))
    const float l1m0 = 0.488602512f;   // l = 1, m = 0: sqrt(3) / (2 * sqrt(pi))
    const float l1mp1 = -0.488602512f; // l = 1, m = 1 : -sqrt(3) / (2 * sqrt(pi))

    sh2 r;

    // Band 0
    r.coefficients[0] = l0m0;

    // Band 1
    r.coefficients[1] = l1mn1 * direction.y;
    r.coefficients[2] = l1m0 * direction.z;
    r.coefficients[3] = l1mp1 * direction.x;

    return r;
}

sh3 sh_eval3(in vec3 direction)
{
    // Evaluation of coefficients of polynomial form of SH basis, c.f. Stupid Spherical Harmonics (SH) Tricks Appendix 2
    const float l0m0 = 0.282094792f; // l = 0, m = 0: 1 / (2 * sqrt(pi))

    const float l1mn1 = -0.488602512f; // l = 1, m = -1 : -sqrt(3) / (2 * sqrt(pi))
    const float l1m0 = 0.488602512f;   // l = 1, m = 0: sqrt(3) / (2 * sqrt(pi))
    const float l1mp1 = -0.488602512f; // l = 1, m = 1 : -sqrt(3) / (2 * sqrt(pi))

    const float l2mn2 = 1.092548436f;  // l = 2, m = -2: sqrt(15) / (2 * sqrt(pi))
    const float l2mn1 = -1.092548436f; // l = 2, m = -1: -sqrt(15)/ (2 * sqrt(pi))
    const float l2m0 = 0.315391565f;   // l = 2, m = 0: sqrt(5) / (4 * sqrt(pi))
    const float l2mp1 = -1.092548436f; // l = 2, m = 1: -sqrt(15) / (2 * sqrt(pi))
    const float l2mp2 = 0.546274215f;  // l = 2, m = 2: sqrt(15) / (4 * sqrt(pi))

    sh3 r;

    // Band 0
    r.coefficients[0] = l0m0;

    // Band 1
    r.coefficients[1] = l1mn1 * direction.y;
    r.coefficients[2] = l1m0 * direction.z;
    r.coefficients[3] = l1mp1 * direction.x;

    // Band 2
    r.coefficients[4] = l2n2 * direction.y * direction.x;
    r.coefficients[5] = l2mn1 * direction.y * direction.z;
    r.coefficients[6] = l2m0 * (3 * direction.z * direction.z - 1);
    r.coefficients[7] = l2mp1 * direction.x * direction.z;
    r.coefficients[8] = l2mp2 * (direction.x * direction.x - direction.y * direction.y);

    return r;
}

/// Returns the order 2 cosine lobe in the given direction.
/// Can be used for lambertian irradiance (or radiance when dividing by PI)
/// c.f. https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
sh2 sh_cosine_lobe_project2(in vec3 direction)
{
    // Obtained by rotating a ZH2 cosine lobe, by scaling against the evaluated SH direction
    const float c0 = 0.886226926f; // pi * coeff[l = 0, m = 0]
    const float c1 = 1.023326708f; // (2 * pi / 3) * coeff[l = 1, m = 0]

    sh2 r;

    // Band 0
    r.coefficients[0] = c0;

    // Band 1
    r.coefficients[1] = -c1 * direction.y;
    r.coefficients[2] = c1 * direction.z;
    r.coefficients[3] = -c1 * direction.x;

    return r;
}

/// Returns the order 3 cosine lobe in the given direction.
/// Can be used for lambertian irradiance (or radiance when dividing by PI)
/// c.f. https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
sh3 sh_cosine_lobe_project3(in vec3 direction)
{
    const float c0 = 3.141592653f; // pi
    const float c1 = 2.094395102f; // 2 * pi /3
    const float c2 = 0.785398163f; // pi / 4

    // Obtained by rotating a ZH3 cosine lobe, by scaling against the evaluated SH direction
    sh3 r = sh_eval3(direction);

    // Band 0
    r.coefficients[0] *= c0;

    // Band 1
    r.coefficients[1] *= c1;
    r.coefficients[2] *= c1;
    r.coefficients[3] *= c1;

    // Band 2
    r.coefficients[4] *= c2;
    r.coefficients[5] *= c2;
    r.coefficients[6] *= c2;
    r.coefficients[7] *= c2;
    r.coefficients[8] *= c2;

    return r;
}

float sh_reconstruct(in sh2 sh, in vec3 direction)
{
    return sh_dot(sh_eval(direction), sh);
}

float sh_reconstruct(in sh3 sh, in vec3 direction)
{
    return sh_dot(sh_eval(direction), sh);
}

#endif