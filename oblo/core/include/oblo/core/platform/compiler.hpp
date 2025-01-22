#pragma once

#ifdef _MSC_VER
    #define OBLO_FORCEINLINE __forceinline
    #define OBLO_NOINLINE __declspec(noinline)

    #ifndef __clang__
        #define OBLO_INTRINSIC [[msvc::intrinsic]]
        #define OBLO_FORCEINLINE_LAMBDA [[msvc::forceinline]]
    #else
        #define OBLO_INTRINSIC
        #define OBLO_FORCEINLINE_LAMBDA
    #endif

#endif