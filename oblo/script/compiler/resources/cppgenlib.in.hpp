#pragma once

#ifdef _MSC_VER
    #define OBLO_SHARED_LIBRARY_EXPORT __declspec(dllexport)
    #define OBLO_SHARED_LIBRARY_IMPORT __declspec(dllimport)
#elif defined(__clang__) or defined(__GNUC__)
    #define OBLO_SHARED_LIBRARY_EXPORT __attribute__((visibility("default")))
    #define OBLO_SHARED_LIBRARY_IMPORT
#endif

#ifdef _WIN32
// This is required to use floats on Windows.
extern "C" int _fltused = 0;

extern "C" OBLO_SHARED_LIBRARY_EXPORT int _DllMainCRTStartup(void*, unsigned, void*)
{
    return 1;
}
#endif

using f32 = float;
using f64 = double;
using i32 = int;
using i64 = long long;
using u32 = unsigned;
using u64 = unsigned long long;

static_assert(sizeof(f32) == 4u);
static_assert(sizeof(f64) == 8u);
static_assert(sizeof(i32) == 4u);
static_assert(sizeof(i64) == 8u);
static_assert(sizeof(u32) == 4u);
static_assert(sizeof(u64) == 8u);

using function_loader_fn = void* (*) (const char*);

//     namespace
//     {
//         f32 (*__intrin_cos)(f32 x);
//         f32 (*__intrin_sin)(f32 x);
//     }

// #define OBLO_LOAD_FUNCTION(Loader, Function) (Function = reinterpret_cast<decltype(Function)>(Loader(#Function)))

// extern "C" OBLO_SHARED_LIBRARY_EXPORT oblo::cppgenlib::i32 oblo_load_functions(
//     oblo::cppgenlib::function_loader_fn loader)
// {
//     if (!loader)
//     {
//         return 0;
//     }

//     const bool success = OBLO_LOAD_FUNCTION(__intrin_cos) != nullptr && OBLO_LOAD_FUNCTION(__intrin_sin) != nullptr;

//     return oblo::cppgenlib::i32{success};
// }