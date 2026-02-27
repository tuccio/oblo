#ifndef OBLO_INCLUDE_RENDERER_CONSTANTS
#define OBLO_INCLUDE_RENDERER_CONSTANTS

float float_positive_infinity()
{
    return uintBitsToFloat(0x7F800000);
}

float float_negative_infinity()
{
    return uintBitsToFloat(0xFF800000);
}

float float_pi()
{
    return 3.1415926535897931;
}

float float_inv_pi()
{
    return 0.31830988618379069;
}

#endif