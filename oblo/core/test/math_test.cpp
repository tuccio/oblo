#include <gtest/gtest.h>

#include <oblo/math/quaternion.hpp>

#include <Eigen/Geometry>
#include <unsupported/Eigen/EulerAngles>

#include <iterator>
#include <random>

namespace oblo
{
    namespace
    {
        constexpr f32 Tollerance{.00001f};

        vec3 random_vec3(std::default_random_engine& rng, std::uniform_real_distribution<float>& f32Dist)
        {
            return {
                .x = f32Dist(rng),
                .y = f32Dist(rng),
                .z = f32Dist(rng),
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

        void assert_near(const quaternion& o, const Eigen::Quaternionf& e, const f32 tollerance = Tollerance)
        {
            ASSERT_NEAR(o.x, e.x(), tollerance);
            ASSERT_NEAR(o.y, e.y(), tollerance);
            ASSERT_NEAR(o.z, e.z(), tollerance);
            ASSERT_NEAR(o.w, e.w(), tollerance);
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

            ASSERT_NEAR(norm(q1), e1.norm(), Tollerance);
            ASSERT_NEAR(norm(q2), e2.norm(), Tollerance);

            assert_near(normalize(q1), e1.normalized());
            assert_near(normalize(q2), e2.normalized());
        }

        for (u32 i = 0; i < N; ++i)
        {
            const vec3 axis = normalize(random_vec3(rng, f32Dist));
            const radians angle{f32Dist(rng)};

            const quaternion q = quaternion::from_axis_angle(axis, angle);
            ASSERT_NEAR(norm(q), 1.f, Tollerance);

            const Eigen::Quaternionf e = from_oblo(q);
            assert_near(q, e);

            ASSERT_NEAR(dot(q, q), 1.f, Tollerance);
            ASSERT_NEAR(dot(q, q), e.dot(e), Tollerance);
        }
    }

    TEST(quaternion, rotation)
    {
        std::default_random_engine rng{42};
        std::uniform_real_distribution<float> f32Dist;

        constexpr u32 N = 1024;

        for (u32 i = 0; i < N; ++i)
        {
            const vec3 angles = random_vec3(rng, f32Dist);

            const auto qXYZ = quaternion::from_euler_xyz_intrinsic(radians_tag{}, angles);
            const auto qZYX = quaternion::from_euler_zyx_intrinsic(radians_tag{}, angles);

            const Eigen::EulerAngles<f32, Eigen::EulerSystemXYZ> eAnglesXYZ(angles.x, angles.y, angles.z);
            const Eigen::EulerAngles<f32, Eigen::EulerSystemZYX> eAnglesZYX(angles.x, angles.y, angles.z);

            const Eigen::Quaternionf eXYZ(eAnglesXYZ);
            const Eigen::Quaternionf eZYX(eAnglesZYX);

            ASSERT_NEAR(eXYZ.dot(from_oblo(qXYZ)), 1.f, Tollerance);
            ASSERT_NEAR(eZYX.dot(from_oblo(qZYX)), 1.f, Tollerance);
        }
    }
}