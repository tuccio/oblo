#include <gtest/gtest.h>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/descriptors/native_asset_descriptor.hpp>
#include <oblo/asset/import/copy_importer.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/properties/serialization/common.hpp>
#include <oblo/thread/job_manager.hpp>

namespace oblo
{
    namespace
    {
        struct mock_test_asset : string_builder
        {
        };

        native_asset_descriptor make_text_asset_desc()
        {
            return {
                .typeUuid = "9902b14d-32f6-4bf3-943c-8b6622591b33"_uuid,
                .typeId = get_type_id<string_builder>(),
                .fileExtension = ".txt",
                .load =
                    [](any_asset& asset, cstring_view source, const any&)
                {
                    auto& b = asset.emplace<mock_test_asset>();
                    return filesystem::load_text_file_into_memory(b, source).has_value();
                },
                .save =
                    [](const any_asset& asset, cstring_view destination, cstring_view, const any&)
                {
                    auto* const b = asset.as<mock_test_asset>();

                    if (!b)
                    {
                        return false;
                    }

                    return filesystem::write_file(destination, as_bytes(std::span{*b}), {}).has_value();
                },
                .createImporter = []() -> unique_ptr<file_importer>
                { return allocate_unique<copy_importer>("3c06e01d-fb44-442d-8f62-3e6e4d14f74d"_uuid, "text"); },
            };
        }

        bool check_text_asset_content(asset_registry& registry, uuid assetId, string_view expected)
        {
            auto r = registry.load_asset(assetId);

            if (!r)
            {
                return false;
            }

            auto* const b = r->as<mock_test_asset>();

            if (!b)
            {
                return false;
            }

            return b->view() == expected;
        }

        bool find_artifact_path(asset_registry& registry, uuid assetId, string_builder& artifactPath)
        {
            dynamic_array<uuid> artifacts;

            if (!registry.find_asset_artifacts(assetId, artifacts) || artifacts.size() != 1)
            {
                return false;
            }

            return registry.get_artifact_path(artifacts[0], artifactPath);
        }

        bool check_text_artifact_content(asset_registry& registry, uuid assetId, string_view expected)
        {
            string_builder artifactPath;

            if (!find_artifact_path(registry, assetId, artifactPath))
            {
                return false;
            }

            string_builder content;

            if (!filesystem::load_text_file_into_memory(content, artifactPath))
            {
                return false;
            }

            return content.view() == expected;
        }

        bool setup_registry(asset_registry& registry, cstring_view directory)
        {
            string_builder assetsDir;
            assetsDir.append(directory).append_path("assets");

            string_builder artifactsDir;
            artifactsDir.append(directory).append_path("artifacts");

            string_builder sourceFiles;
            sourceFiles.append(directory).append_path("sources");

            const asset_source_descriptor assetSources[]{
                {
                    .id = "assets"_hsv,
                    .assetsDirectory = assetsDir,
                    .sourcesDirectory = sourceFiles,
                },
            };

            if (!registry.initialize(assetSources, artifactsDir))
            {
                return false;
            }

            return true;
        }

        void wait_processing(asset_registry& registry)
        {
            do
            {
                registry.update();
            } while (registry.get_running_import_count() > 0);
        }
    }

    std::ostream& operator<<(std::ostream& stream, const string_builder& b)
    {
        return stream << b.c_str();
    }

    template <>
    struct asset_traits<mock_test_asset>
    {
        static constexpr uuid uuid = "9902b14d-32f6-4bf3-943c-8b6622591b33"_uuid;
    };

    TEST(asset_registry, rename_with_registry_running)
    {
        job_manager jm;
        jm.init();

        const auto cleanup = finally([&jm] { jm.shutdown(); });

        constexpr cstring_view dir = "./rename_with_registry_running";
        constexpr cstring_view assetsDir = "./rename_with_registry_running/assets";

        filesystem::remove_all(dir).assert_value();

        asset_registry registry;

        ASSERT_TRUE(setup_registry(registry, dir));

        registry.register_native_asset_type(make_text_asset_desc());

        registry.discover_assets({});

        uuid assetA;

        {
            any_asset a;
            auto& b = a.emplace<mock_test_asset>();
            b.append("A");

            const auto id = registry.create_asset(a, "$assets/", "A");

            ASSERT_TRUE(id);
            assetA = *id;
        }

        wait_processing(registry);

        ASSERT_TRUE(check_text_asset_content(registry, assetA, "A"));

        {
            string_builder expectedPath;
            expectedPath.append(registry.get_asset_directory("assets"_hsv));

            string_builder assetPath;
            ASSERT_TRUE(registry.get_asset_directory(assetA, assetPath));
            ASSERT_EQ(assetPath, expectedPath);

            string_builder assetName;
            ASSERT_TRUE(registry.get_asset_name(assetA, assetName));
            ASSERT_EQ(assetName.view(), "A");
        }

        {
            string_builder newDir;
            newDir.append(assetsDir).append_path("New Dir");

            ASSERT_TRUE(filesystem::create_directories(newDir));

            string_builder previousFile;
            previousFile.append(assetsDir).append_path("A").append(AssetMetaExtension);

            string_builder newFile;
            newFile.append(newDir).append_path("A").append(AssetMetaExtension);

            ASSERT_TRUE(filesystem::rename(previousFile, newFile));
        }

        wait_processing(registry);

        {
            string_builder expectedPath;
            expectedPath.append(registry.get_asset_directory("assets"_hsv)).append_path("New Dir");

            string_builder assetPath;
            ASSERT_TRUE(registry.get_asset_directory(assetA, assetPath));
            ASSERT_EQ(assetPath, expectedPath);

            string_builder assetName;
            ASSERT_TRUE(registry.get_asset_name(assetA, assetName));
            ASSERT_EQ(assetName.view(), "A");
        }

        ASSERT_TRUE(check_text_asset_content(registry, assetA, "A"));

        {
            // Move it back but change name
            string_builder newDir;
            newDir.append(assetsDir).append_path("New Dir");

            ASSERT_TRUE(filesystem::create_directories(newDir));

            string_builder oldName;
            oldName.append(newDir).append_path("A").append(AssetMetaExtension);

            string_builder newName;
            newName.append(assetsDir).append_path("A_renamed").append(AssetMetaExtension);

            ASSERT_TRUE(filesystem::rename(oldName, newName));
        }

        wait_processing(registry);

        {
            string_builder expectedPath;
            expectedPath.append(registry.get_asset_directory("assets"_hsv));

            string_builder assetPath;
            ASSERT_TRUE(registry.get_asset_directory(assetA, assetPath));
            ASSERT_EQ(assetPath, expectedPath);

            string_builder assetName;
            ASSERT_TRUE(registry.get_asset_name(assetA, assetName));
            ASSERT_EQ(assetName.view(), "A_renamed");
        }

        ASSERT_TRUE(check_text_asset_content(registry, assetA, "A"));
    }

    class asset_registry_import : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            jm.init();

            constexpr cstring_view dir = "./import";
            constexpr cstring_view file = "./import/file.txt";

            constexpr string_view extensions[]{".txt"};

            filesystem::remove_all(dir).assert_value();

            ASSERT_TRUE(setup_registry(registry, dir));

            registry.register_file_importer({
                .type = get_type_id<mock_test_asset>(),
                .create = []() -> unique_ptr<file_importer>
                { return allocate_unique<copy_importer>("3c06e01d-fb44-442d-8f62-3e6e4d14f74d"_uuid, "text"); },
                .extensions = extensions,
            });

            registry.discover_assets({});

            constexpr string_view content = "Content of the file";
            write_file(file, content);

            {
                const auto id = registry.import(file, "$assets/", "A", {});

                ASSERT_TRUE(id);
                assetA = *id;
            }

            wait_processing(registry);

            ASSERT_TRUE(check_text_artifact_content(registry, assetA, content));

            {
                string_builder expectedPath;
                expectedPath.append(registry.get_asset_directory("assets"_hsv));

                string_builder assetPath;
                ASSERT_TRUE(registry.get_asset_directory(assetA, assetPath));
                ASSERT_EQ(assetPath, expectedPath);

                string_builder assetName;
                ASSERT_TRUE(registry.get_asset_name(assetA, assetName));
                ASSERT_EQ(assetName.view(), "A");
            }
        }

        void TearDown() override
        {
            jm.shutdown();
        }

        void write_file(cstring_view file, string_view content)
        {
            ASSERT_TRUE(filesystem::write_file(file, as_bytes(std::span{content.data(), content.size()}), {}));
        }

        job_manager jm;
        asset_registry registry;
        uuid assetA{};
    };

    TEST_F(asset_registry_import, basic) {}

    TEST_F(asset_registry_import, reimport_same_artifacts)
    {
        string_builder sourcePath;

        ASSERT_TRUE(registry.get_source_directory(assetA, sourcePath));
        sourcePath.append_path("file.txt");

        constexpr string_view newContent = "New content";
        write_file(sourcePath.view(), newContent);

        ASSERT_TRUE(registry.process(assetA));

        wait_processing(registry);

        ASSERT_TRUE(check_text_artifact_content(registry, assetA, newContent));

        {
            string_builder expectedPath;
            expectedPath.append(registry.get_asset_directory("assets"_hsv));

            string_builder assetPath;
            ASSERT_TRUE(registry.get_asset_directory(assetA, assetPath));
            ASSERT_EQ(assetPath, expectedPath);

            string_builder assetName;
            ASSERT_TRUE(registry.get_asset_name(assetA, assetName));
            ASSERT_EQ(assetName.view(), "A");
        }
    }
}