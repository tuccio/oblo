#include <gtest/gtest.h>

#include <oblo/math/vec3.hpp>
#include <oblo/scene/assets/material.hpp>

namespace oblo
{
    namespace
    {
        void clear_directory(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            ASSERT_FALSE(ec);
        }
    }

    TEST(materials, serialization)
    {
        const std::filesystem::path testDir{"./test/materials_serialization/"};
        clear_directory(testDir);

        std::filesystem::create_directories(testDir);

        constexpr vec3 albedo{.25f, .5f, .1f};
        constexpr f32 roughness{.7f};
        constexpr f32 metalness{.3f};

        {
            material m;

            m.set_property("Albedo", albedo);
            m.set_property("Roughness", roughness);
            m.set_property("Metalness", metalness);

            m.save(testDir / "material.json");
        }

        {
            material m;

            ASSERT_TRUE(m.load(testDir / "material.json"));
            auto* const albedoProperty = m.get_property("Albedo");
            ASSERT_TRUE(albedoProperty);

            auto* const roughnessProperty = m.get_property("Roughness");
            ASSERT_TRUE(albedoProperty);

            auto* const metalnessProperty = m.get_property("Metalness");
            ASSERT_TRUE(albedoProperty);

            const auto rAlbedo = albedoProperty->as<vec3>();
            const auto rRoughness = roughnessProperty->as<f32>();
            const auto rMetalness = metalnessProperty->as<f32>();

            ASSERT_TRUE(rAlbedo);
            ASSERT_TRUE(rRoughness);
            ASSERT_TRUE(rMetalness);

            constexpr f32 Tollerance{.0001f};

            ASSERT_NEAR(albedo.x, rAlbedo->x, Tollerance);
            ASSERT_NEAR(albedo.y, rAlbedo->y, Tollerance);
            ASSERT_NEAR(albedo.z, rAlbedo->z, Tollerance);

            ASSERT_NEAR(roughness, *rRoughness, Tollerance);
            ASSERT_NEAR(metalness, *rMetalness, Tollerance);
        }
    }
}