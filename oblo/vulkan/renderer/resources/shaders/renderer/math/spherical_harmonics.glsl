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

sh2 sh2_zero()
{
    sh2 r;
    r.coefficients = vec4(0);
    return r;
}

sh3 sh3_zero()
{
    sh3 r;

    r.coefficients[0] = 0;
    r.coefficients[1] = 0;
    r.coefficients[2] = 0;
    r.coefficients[3] = 0;
    r.coefficients[4] = 0;
    r.coefficients[5] = 0;
    r.coefficients[6] = 0;
    r.coefficients[7] = 0;
    r.coefficients[8] = 0;

    return r;
}

sh2 sh_add(in sh2 a, in sh2 b)
{
    sh2 r;
    r.coefficients = a.coefficients + b.coefficients;
    return r;
}

sh3 sh_add(in sh3 a, in sh3 b)
{
    sh3 r;

    r.coefficients[0] = a.coefficients[0] + b.coefficients[0];
    r.coefficients[1] = a.coefficients[1] + b.coefficients[1];
    r.coefficients[2] = a.coefficients[2] + b.coefficients[2];
    r.coefficients[3] = a.coefficients[3] + b.coefficients[3];
    r.coefficients[4] = a.coefficients[4] + b.coefficients[4];
    r.coefficients[5] = a.coefficients[5] + b.coefficients[5];
    r.coefficients[6] = a.coefficients[6] + b.coefficients[6];
    r.coefficients[7] = a.coefficients[7] + b.coefficients[7];
    r.coefficients[8] = a.coefficients[8] + b.coefficients[8];

    return r;
}

float sh_dot(in sh2 a, in sh2 b)
{
    return dot(a.coefficients, b.coefficients);
}

float sh_dot(in sh3 a, in sh3 b)
{
    return a.coefficients[0] * b.coefficients[0] + a.coefficients[1] * b.coefficients[1] +
        a.coefficients[2] * b.coefficients[2] + a.coefficients[3] * b.coefficients[3] +
        a.coefficients[4] * b.coefficients[4] + a.coefficients[5] * b.coefficients[5] +
        a.coefficients[6] * b.coefficients[6] + a.coefficients[7] * b.coefficients[7] +
        a.coefficients[8] * b.coefficients[8];
}

sh2 sh_mul(in sh2 a, in float v)
{
    sh2 r;
    r.coefficients = a.coefficients * v;
    return r;
}

sh3 sh_mul(in sh3 a, in float v)
{
    sh3 r;

    r.coefficients[0] = a.coefficients[0] * v;
    r.coefficients[1] = a.coefficients[1] * v;
    r.coefficients[2] = a.coefficients[2] * v;
    r.coefficients[3] = a.coefficients[3] * v;
    r.coefficients[4] = a.coefficients[4] * v;
    r.coefficients[5] = a.coefficients[5] * v;
    r.coefficients[6] = a.coefficients[6] * v;
    r.coefficients[7] = a.coefficients[7] * v;
    r.coefficients[8] = a.coefficients[8] * v;

    return r;
}

sh2 sh_mix(in sh2 a, in sh2 b, in float k)
{
    return sh_add(sh_mul(a, 1.f - k), sh_mul(b, k));
}

sh3 sh_mix(in sh3 a, in sh3 b, in float k)
{
    return sh_add(sh_mul(a, 1.f - k), sh_mul(b, k));
}

sh2 sh2_eval(in vec3 direction)
{
    // Evaluation of coefficients of polynomial form of SH basis, c.f. Stupid Spherical Harmonics (SH) Tricks
    // Appendix 2
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

sh3 sh3_eval(in vec3 direction)
{
    // Evaluation of coefficients of polynomial form of SH basis, c.f. Stupid Spherical Harmonics (SH) Tricks
    // Appendix 2
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
    r.coefficients[4] = l2mn2 * direction.y * direction.x;
    r.coefficients[5] = l2mn1 * direction.y * direction.z;
    r.coefficients[6] = l2m0 * (3 * direction.z * direction.z - 1);
    r.coefficients[7] = l2mp1 * direction.x * direction.z;
    r.coefficients[8] = l2mp2 * (direction.x * direction.x - direction.y * direction.y);

    return r;
}

/// Returns the order 2 cosine lobe in the given direction.
/// Can be used for lambertian irradiance (or radiance when dividing by PI)
/// c.f. https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
sh2 sh2_cosine_lobe_project(in vec3 direction)
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
sh3 sh3_cosine_lobe_project(in vec3 direction)
{
    const float c0 = 3.141592653f; // pi
    const float c1 = 2.094395102f; // 2 * pi / 3
    const float c2 = 0.785398163f; // pi / 4

    // Obtained by rotating a ZH3 cosine lobe, by scaling against the evaluated SH direction
    sh3 r = sh3_eval(direction);

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

#endif