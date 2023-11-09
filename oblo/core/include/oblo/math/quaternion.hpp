#pragma once

#include <oblo/core/types.hpp>
#include <oblo/math/angle.hpp>
#include <oblo/math/vec3.hpp>

#include <cmath>

namespace oblo
{
    struct quaternion
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;

        constexpr bool operator==(const quaternion&) const = default;

        static constexpr quaternion identity();

        static quaternion from_axis_angle(const vec3& axis, radians angle);

        static quaternion from_euler_xyz_intrinsic(const radians x, const radians y, const radians z);
        static quaternion from_euler_xyz_intrinsic(radians_tag, const vec3& xyz);
        static quaternion from_euler_xyz_intrinsic(degrees_tag, const vec3& xyz);

        static quaternion from_euler_zyx_intrinsic(const radians x, const radians y, const radians z);
        static quaternion from_euler_zyx_intrinsic(radians_tag, const vec3& xyz);
        static quaternion from_euler_zyx_intrinsic(degrees_tag, const vec3& xyz);

        static vec3 to_euler_xyz_intrinsic(radians_tag, const quaternion& q);
        static vec3 to_euler_xyz_intrinsic(degrees_tag, const quaternion& q);

        static vec3 to_euler_zyx_intrinsic(radians_tag, const quaternion& q);
        static vec3 to_euler_zyx_intrinsic(degrees_tag, const quaternion& q);
    };

    constexpr quaternion operator*(const quaternion& lhs, const quaternion& rhs)
    {
        return {
            .x = lhs.x * rhs.w + lhs.w * rhs.x + lhs.y * rhs.z - lhs.z * rhs.y,
            .y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
            .z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
            .w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
        };
    }

    inline quaternion conjugate(const quaternion& q)
    {
        return {.x = -q.x, .y = -q.y, .z = -q.z, .w = q.w};
    }

    constexpr quaternion inverse(const quaternion& q)
    {
        const f32 invSquaredNorm = 1.f / (q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        return {-q.x * invSquaredNorm, -q.y * invSquaredNorm, -q.z * invSquaredNorm, q.w * invSquaredNorm};
    }

    constexpr f32 dot(const quaternion& lhs, const quaternion& rhs)
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
    }

    constexpr f32 norm_squared(const quaternion& q)
    {
        return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    }

    inline f32 norm(const quaternion& q)
    {
        return std::sqrt(norm_squared(q));
    }

    inline quaternion normalize(const quaternion& q)
    {
        const f32 invNorm = 1.f / std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        return {q.x * invNorm, q.y * invNorm, q.z * invNorm, q.w * invNorm};
    }

    constexpr vec3 transform(const quaternion& q, const vec3& v)
    {
        const vec3 u{q.x, q.y, q.z};
        const vec3 uv = 2.f * cross(u, v);
        return v + q.w * uv + cross(u, uv);
    }

    constexpr vec3 operator*(const quaternion& q, const vec3& v)
    {
        return transform(q, v);
    }

    inline quaternion quaternion::from_axis_angle(const vec3& axis, radians angle)
    {
        const f32 halfAngle = angle.value * .5f;

        const f32 sinHalfAngle = std::sin(halfAngle);
        const f32 cosHalfAngle = std::cos(halfAngle);

        return {
            .x = axis.x * sinHalfAngle,
            .y = axis.y * sinHalfAngle,
            .z = axis.z * sinHalfAngle,
            .w = cosHalfAngle,
        };
    }

    constexpr quaternion quaternion::identity()
    {
        return {.w = 1};
    }

    inline quaternion quaternion::from_euler_xyz_intrinsic(const radians x, const radians y, const radians z)
    {
        const auto qx = quaternion::from_axis_angle(vec3{.x = 1.f}, x);
        const auto qy = quaternion::from_axis_angle(vec3{.y = 1.f}, y);
        const auto qz = quaternion::from_axis_angle(vec3{.z = 1.f}, z);

        return qx * qy * qz;
    }

    inline quaternion quaternion::from_euler_xyz_intrinsic(const radians_tag, const vec3& xyz)
    {
        return from_euler_xyz_intrinsic(radians{xyz.x}, radians{xyz.y}, radians{xyz.z});
    }

    inline quaternion quaternion::from_euler_xyz_intrinsic(const degrees_tag, const vec3& xyz)
    {
        return from_euler_xyz_intrinsic(degrees{xyz.x}, degrees{xyz.y}, degrees{xyz.z});
    }

    inline quaternion quaternion::from_euler_zyx_intrinsic(const radians z, const radians y, const radians x)
    {
        const auto qz = quaternion::from_axis_angle(vec3{.z = 1.f}, z);
        const auto qy = quaternion::from_axis_angle(vec3{.y = 1.f}, y);
        const auto qx = quaternion::from_axis_angle(vec3{.x = 1.f}, x);

        return qz * qy * qx;
    }

    inline quaternion quaternion::from_euler_zyx_intrinsic(const radians_tag, const vec3& zyx)
    {
        return from_euler_zyx_intrinsic(radians{zyx.x}, radians{zyx.y}, radians{zyx.z});
    }

    inline quaternion quaternion::from_euler_zyx_intrinsic(const degrees_tag, const vec3& zyx)
    {
        return from_euler_zyx_intrinsic(degrees{zyx.x}, degrees{zyx.y}, degrees{zyx.z});
    }

    inline vec3 quaternion::to_euler_xyz_intrinsic(radians_tag, const quaternion& q)
    {
        const f32 x2 = q.x * q.x;
        const f32 y2 = q.y * q.y;
        const f32 z2 = q.z * q.z;
        const f32 w2 = q.w * q.w;

        const f32 r11 = -2 * (q.y * q.z - q.w * q.x);
        const f32 r12 = w2 - x2 - y2 + z2;
        const f32 r21 = 2 * (q.x * q.z + q.w * q.y);
        const f32 r31 = -2 * (q.x * q.y - q.w * q.z);
        const f32 r32 = w2 + x2 - y2 - z2;

        return vec3{
            std::atan2(r11, r12),
            std::asin(r21),
            std::atan2(r31, r32),
        };
    }

    inline vec3 quaternion::to_euler_xyz_intrinsic(degrees_tag, const quaternion& q)
    {
        const auto rad = to_euler_xyz_intrinsic(radians_tag{}, q);

        return vec3{
            convert(rad.x, radians_tag{}, degrees_tag{}),
            convert(rad.y, radians_tag{}, degrees_tag{}),
            convert(rad.z, radians_tag{}, degrees_tag{}),
        };
    }

    inline vec3 quaternion::to_euler_zyx_intrinsic(radians_tag, const quaternion& q)
    {
        const f32 x2 = q.x * q.x;
        const f32 y2 = q.y * q.y;
        const f32 z2 = q.z * q.z;
        const f32 w2 = q.w * q.w;

        const f32 r11 = 2 * (q.x * q.y + q.w * q.z);
        const f32 r12 = w2 + x2 - y2 - z2;
        const f32 r21 = -2 * (q.x * q.z - q.w * q.y);
        const f32 r31 = 2 * (q.y * q.z + q.w * q.x);
        const f32 r32 = w2 - x2 - y2 + z2;

        return vec3{
            std::atan2(r11, r12),
            std::asin(r21),
            std::atan2(r31, r32),
        };
    }

    inline vec3 quaternion::to_euler_zyx_intrinsic(degrees_tag, const quaternion& q)
    {
        const auto rad = to_euler_zyx_intrinsic(radians_tag{}, q);

        return vec3{
            convert(rad.x, radians_tag{}, degrees_tag{}),
            convert(rad.y, radians_tag{}, degrees_tag{}),
            convert(rad.z, radians_tag{}, degrees_tag{}),
        };
    }

}