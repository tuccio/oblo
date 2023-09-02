#include <gtest/gtest.h>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/scene/assets/registration.hpp>

namespace oblo::asset::importers
{
    TEST(gltf_importer, box)
    {
        asset_registry assetManager;

        const std::filesystem::path assetsDir{"./test/gltf_importer_suzanne/assets"};
        const std::filesystem::path artifactsDir{"./test/gltf_importer_suzanne/artifacts"};

        ASSERT_TRUE(assetManager.initialize(assetsDir, artifactsDir));

        scene::register_asset_types(assetManager);

        register_gltf_importer(assetManager);

        const std::filesystem::path gltfSampleModels{OBLO_GLTF_SAMPLE_MODELS};

        auto importer = assetManager.create_importer(gltfSampleModels / "2.0" / "Box" / "glTF-Embedded" / "Box.gltf");

        ASSERT_TRUE(importer.is_valid());

        ASSERT_TRUE(importer.init());
        ASSERT_TRUE(importer.execute("./Box"));
    }
}