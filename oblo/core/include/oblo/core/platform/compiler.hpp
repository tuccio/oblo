#pragma once

#ifdef _MSC_VER
    #define OBLO_FORCEINLINE __forceinline
    #define OBLO_NOINLINE __declspec(noinline)

    #define OBLO_SHARED_LIBRARY_EXPORT __declspec(dllexport)
    #define OBLO_SHARED_LIBRARY_IMPORT __declspec(dllimport)

    #ifndef __clang__
        #define OBLO_INTRINSIC [[msvc::intrinsic]]
        #define OBLO_FORCEINLINE_LAMBDA [[msvc::forceinline]]
    #else
        #define OBLO_INTRINSIC
        #define OBLO_FORCEINLINE_LAMBDA
    #endif

#elif defined(__clang__)
    // Clang-specific definitions
    #define OBLO_FORCEINLINE inline __attribute__((always_inline))
    #define OBLO_NOINLINE __attribute__((noinline))

    // Define shared library export/import attributes
    #define OBLO_SHARED_LIBRARY_EXPORT __attribute__((visibility("default")))
    #define OBLO_SHARED_LIBRARY_IMPORT

    // Clang doesn't have MSVC's `intrinsic`, so we just leave it empty or define something custom
    #define OBLO_INTRINSIC
    #define OBLO_FORCEINLINE_LAMBDA inline __attribute__((always_inline))

#endif