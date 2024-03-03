#ifndef OBLO_INCLUDE_RENDERER_CONSTANTS
#define OBLO_INCLUDE_RENDERER_CONSTANTS

float make_positive_infinity()
{
    return uintBitsToFloat(0x7F800000);;
}

float make_negative_infinity()
{
    return uintBitsToFloat(0xFF800000);;
}

#endif