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

#endif