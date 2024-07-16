#pragma once

#ifdef _MSC_VER
#define OBLO_FORCEINLINE __forceinline
#define OBLO_NOINLINE __declspec(noinline)
#define OBLO_INTRINSIC [[msvc::intrinsic]]
#endif