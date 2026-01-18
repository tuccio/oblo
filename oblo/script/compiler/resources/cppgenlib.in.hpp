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

using cstring = const char*;

static_assert(sizeof(f32) == 4u);
static_assert(sizeof(f64) == 8u);
static_assert(sizeof(i32) == 4u);
static_assert(sizeof(i64) == 8u);
static_assert(sizeof(u32) == 4u);
static_assert(sizeof(u64) == 8u);

using symbol_loader_fn = void* (*) (const char*);

// vec3

struct vec3
{
    float x, y, z;
};

constexpr vec3 operator-(const vec3& lhs, const vec3& rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

constexpr vec3 operator-(f32 lhs, const vec3& rhs) noexcept
{
    return {lhs - rhs.x, lhs - rhs.y, lhs - rhs.z};
}

constexpr vec3 operator-(const vec3& lhs, f32 rhs) noexcept
{
    return {lhs.x - rhs, lhs.y - rhs, lhs.z - rhs};
}

constexpr vec3 operator+(const vec3& lhs, const vec3& rhs) noexcept
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

constexpr vec3 operator+(f32 lhs, const vec3& rhs) noexcept
{
    return {lhs + rhs.x, lhs + rhs.y, lhs + rhs.z};
}

constexpr vec3 operator+(const vec3& lhs, f32 rhs) noexcept
{
    return {lhs.x + rhs, lhs.y + rhs, lhs.z + rhs};
}

constexpr vec3 operator*(const vec3& lhs, const vec3& rhs) noexcept
{
    return {lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
}

constexpr vec3 operator*(const vec3& lhs, f32 rhs) noexcept
{
    return lhs * vec3{rhs, rhs, rhs};
}

constexpr vec3 operator*(f32 lhs, const vec3& rhs) noexcept
{
    return rhs * lhs;
}

constexpr vec3 operator/(const vec3& lhs, const vec3& rhs) noexcept
{
    return {lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z};
}

constexpr vec3 operator/(f32 lhs, const vec3& rhs) noexcept
{
    return vec3{lhs, lhs, lhs} / rhs;
}

constexpr vec3 operator/(const vec3& lhs, f32 rhs) noexcept
{
    return lhs / vec3{rhs, rhs, rhs};
}
