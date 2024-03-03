#include <gtest/gtest.h>

#include <oblo/math/mat4.hpp>
#include <oblo/math/quaternion.hpp>

#include <Eigen/Geometry>
#include <unsupported/Eigen/EulerAngles>

#include <iterator>
#include <random>

namespace oblo
{
    namespace
    {
        constexpr f32 Tolerance{.00001f};

        vec3 random_vec3(std::default_random_engine& rng, std::uniform_real_distribution<float>& f32Dist)
        {
            return {
                .x = f32Dist(rng),
                .y = f32Dist(rng),
                .z = f32Dist(rng),
            };
        }

        vec4 random_vec4(std::default_random_engine& rng, std::uniform_real_distribution<float>& f32Dist)
        {
            return {
                .x = f32Dist(rng),
                .y = f32Dist(rng),
                .z = f32Dist(rng),
                .w = f32Dist(rng),
            };
        }

        quaternion random_quaternion(std::default_random_engine& rng, std::uniform_real_distribution<float>& f32Dist)
        {
            return {
                .x = f32Dist(rng),
                .y = f32Dist(rng),
                .z = f32Dist(rng),
                .w = f32Dist(rng),
            };
        }

        Eigen::Quaternionf from_oblo(const quaternion& q)
        {
            return Eigen::Quaternionf{q.w, q.x, q.y, q.z};
        }

        Eigen::Matrix4f from_oblo(const mat4& m)
        {
            Eigen::Matrix4f e;

            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    e(i, j) = m[i][j];
                }
            }

            return e;
        }

        void assert_near(const quaternion& o, const Eigen::Quaternionf& e, const f32 tolerance = Tolerance)
        {
            ASSERT_NEAR(o.x, e.x(), tolerance);
            ASSERT_NEAR(o.y, e.y(), tolerance);
            ASSERT_NEAR(o.z, e.z(), tolerance);
            ASSERT_NEAR(o.w, e.w(), tolerance);
        }

        void assert_near(const vec4& o, const Eigen::Vector4f& e, const f32 tolerance = Tolerance)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                ASSERT_NEAR(e[i], o[i], tolerance);
            }
        }

        void assert_near(const mat4& o, const Eigen::Matrix4f& e, const f32 tolerance = Tolerance)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    ASSERT_NEAR(e(i, j), o[i][j], tolerance);
                }
            }
        }

        void assert_near_adaptive(f32 a, f32 b)
        {
            // The larger the number, the bigger the error
            const f32 p = 2.f - std::log10(max(std::abs(a), std::abs(b)));
            const f32 e = std::pow(.1f, std::floor(p));

            ASSERT_NEAR(a, b, e);
        }

        void assert_near_adaptive(const mat4& o, const Eigen::Matrix4f& e)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                for (u32 j = 0; j < 4; ++j)
                {
                    assert_near_adaptive(e(i, j), o[i][j]);
                }
            }
        }

        void assert_near_adaptive(const vec4& o, const Eigen::Vector4f& e)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                assert_near_adaptive(e[i], o[i]);
            }
        }
    }

    TEST(quaternion, basic_operations)
    {
        std::default_random_engine rng{42};
        std::uniform_real_distribution<float> f32Dist;

        constexpr u32 N = 1024;

        for (u32 i = 0; i < N; ++i)
        {
            const quaternion q1 = random_quaternion(rng, f32Dist);
            const quaternion q2 = random_quaternion(rng, f32Dist);

            const Eigen::Quaternionf e1 = from_oblo(q1);
            const Eigen::Quaternionf e2 = from_oblo(q2);

            assert_near(q1 * q2, e1 * e2);
            assert_near(q2 * q1, e2 * e1);

            ASSERT_NEAR(norm(q1), e1.norm(), Tolerance);
            ASSERT_NEAR(norm(q2), e2.norm(), Tolerance);

            assert_near(normalize(q1), e1.normalized());
            assert_near(normalize(q2), e2.normalized());
        }

        for (u32 i = 0; i < N; ++i)
        {
            const vec3 axis = normalize(random_vec3(rng, f32Dist));
            const radians angle{f32Dist(rng)};

            const quaternion q = quaternion::from_axis_angle(axis, angle);
            ASSERT_NEAR(norm(q), 1.f, Tolerance);

            const Eigen::Quaternionf e = from_oblo(q);
            assert_near(q, e);

            ASSERT_NEAR(dot(q, q), 1.f, Tolerance);
            ASSERT_NEAR(dot(q, q), e.dot(e), Tolerance);
        }
    }

    TEST(quaternion, rotation)
    {
        std::default_random_engine rng{42};
        std::uniform_real_distribution<float> f32Dist{-pi, pi};

        constexpr u32 N = 1024;

        for (u32 i = 0; i < N; ++i)
        {
            const vec3 angles = random_vec3(rng, f32Dist);

            const auto qXYZ = quaternion::from_euler_xyz_intrinsic(radians_tag{}, angles);
            const auto qZYX = quaternion::from_euler_zyx_intrinsic(radians_tag{}, angles);

            ASSERT_NEAR(norm(qXYZ), 1.f, Tolerance);
            ASSERT_NEAR(norm(qZYX), 1.f, Tolerance);

            const Eigen::EulerAngles<f32, Eigen::EulerSystemXYZ> eAnglesXYZ(angles.x, angles.y, angles.z);
            const Eigen::EulerAngles<f32, Eigen::EulerSystemZYX> eAnglesZYX(angles.x, angles.y, angles.z);

            const Eigen::Quaternionf eXYZ(eAnglesXYZ);
            const Eigen::Quaternionf eZYX(eAnglesZYX);

            ASSERT_NEAR(std::abs(eXYZ.dot(from_oblo(qXYZ))), 1.f, Tolerance);
            ASSERT_NEAR(std::abs(eZYX.dot(from_oblo(qZYX))), 1.f, Tolerance);

            // Go back to euler, then back to quaternion and check if the rotation still matches

            {
                const auto rZYX = quaternion::to_euler_zyx_intrinsic(radians_tag{}, qZYX);
                const auto fZYX = quaternion::from_euler_zyx_intrinsic(radians_tag{}, rZYX);

                ASSERT_NEAR(std::abs(dot(qZYX, fZYX)), 1.f, Tolerance);
            }

            {
                const auto rXYZ = quaternion::to_euler_xyz_intrinsic(radians_tag{}, qXYZ);
                const auto fXYZ = quaternion::from_euler_xyz_intrinsic(radians_tag{}, rXYZ);

                ASSERT_NEAR(std::abs(dot(qXYZ, fXYZ)), 1.f, Tolerance);
            }
        }
    }

    TEST(mat4, multiply)
    {
        std::default_random_engine rng{42};
        std::uniform_real_distribution<float> f32Dist{-1, 1};

        constexpr u32 N = 1024;

        for (u32 i = 0; i < N; ++i)
        {
            mat4 m1, m2;

            for (auto& row : m1.rows)
            {
                row = random_vec4(rng, f32Dist);
            }

            for (auto& row : m2.rows)
            {
                row = random_vec4(rng, f32Dist);
            }

            const Eigen::Matrix4f eM1 = from_oblo(m1);
            const Eigen::Matrix4f eM2 = from_oblo(m2);

            assert_near(m1 * m2, eM1 * eM2);
        }
    }

    TEST(mat4, inverse)
    {
        std::default_random_engine rng{42};
        std::uniform_real_distribution<float> f32Dist{-1, 1};

        constexpr u32 N = 1024;

        for (u32 i = 0; i < N; ++i)
        {
            mat4 mat;

            for (auto& row : mat.rows)
            {
                row = random_vec4(rng, f32Dist);
            }

            const Eigen::Matrix4f eMat = from_oblo(mat);

            assert_near(mat, eMat, epsilon);

            f32 det;
            const expected<mat4> inv = inverse(mat, &det);

            Eigen::Matrix4f eInv;

            bool isInvertible;
            f32 eDet;
            eMat.computeInverseAndDetWithCheck(eInv, eDet, isInvertible, epsilon);

            ASSERT_EQ(isInvertible, inv.has_value());

            if (isInvertible)
            {
                assert_near_adaptive(det, eDet);
                assert_near_adaptive(*inv, eInv);

                const vec4 p = *inv * vec4::splat(1.f);
                const Eigen::Vector4f e = eInv * Eigen::Vector4f::Ones();

                assert_near_adaptive(p, e);
            }
        }
    }
}