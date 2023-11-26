#include <gtest/gtest.h>

#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/pbr_properties.hpp>

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

        constexpr resource_ref<texture> albedoTexture{
            .id = uuid::parse("6dce4d17-5354-4b1c-a05a-d2547966dbb9").value_or({})};

        constexpr resource_ref<texture> mrTexture{
            .id = uuid::parse("ea53dc3a-742e-402c-a9ff-dd027b8c04dd").value_or({})};

        ASSERT_FALSE(albedoTexture.id.is_nil());
        ASSERT_FALSE(mrTexture.id.is_nil());

        {
            material m;

            m.set_property(pbr::Albedo, albedo);
            m.set_property(pbr::Roughness, roughness);
            m.set_property(pbr::Metalness, metalness);
            m.set_property(pbr::AlbedoTexture, albedoTexture);
            m.set_property(pbr::MetalnessRoughnessTexture, mrTexture);

            m.save(testDir / "material.json");
        }

        {
            material m;

            ASSERT_TRUE(m.load(testDir / "material.json"));
            auto* const albedoProperty = m.get_property(pbr::Albedo);
            ASSERT_TRUE(albedoProperty);

            auto* const roughnessProperty = m.get_property(pbr::Roughness);
            ASSERT_TRUE(roughnessProperty);

            auto* const metalnessProperty = m.get_property(pbr::Metalness);
            ASSERT_TRUE(metalnessProperty);

            auto* const albedoTextureProperty = m.get_property(pbr::AlbedoTexture);
            ASSERT_TRUE(albedoTextureProperty);

            auto* const mrTextureProperty = m.get_property(pbr::MetalnessRoughnessTexture);
            ASSERT_TRUE(mrTextureProperty);

            const auto rAlbedo = albedoProperty->as<vec3>();
            const auto rRoughness = roughnessProperty->as<f32>();
            const auto rMetalness = metalnessProperty->as<f32>();
            const auto rAlbedoTexture = albedoTextureProperty->as<resource_ref<texture>>();
            const auto rMrTextureTexture = mrTextureProperty->as<resource_ref<texture>>();

            ASSERT_TRUE(rAlbedo);
            ASSERT_TRUE(rRoughness);
            ASSERT_TRUE(rMetalness);
            ASSERT_TRUE(rAlbedoTexture);
            ASSERT_TRUE(rMrTextureTexture);

            constexpr f32 Tollerance{.0001f};

            ASSERT_NEAR(albedo.x, rAlbedo->x, Tollerance);
            ASSERT_NEAR(albedo.y, rAlbedo->y, Tollerance);
            ASSERT_NEAR(albedo.z, rAlbedo->z, Tollerance);

            ASSERT_NEAR(roughness, *rRoughness, Tollerance);
            ASSERT_NEAR(metalness, *rMetalness, Tollerance);

            ASSERT_EQ(rAlbedoTexture->id, albedoTexture.id);
            ASSERT_EQ(rMrTextureTexture->id, mrTexture.id);
        }
    }
}