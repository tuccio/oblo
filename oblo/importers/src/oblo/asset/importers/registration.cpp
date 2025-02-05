#include <oblo/asset/importers/registration.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/descriptors/file_importer_descriptor.hpp>
#include <oblo/asset/importers/assimp.hpp>
#include <oblo/asset/importers/dds.hpp>
#include <oblo/asset/importers/gltf.hpp>
#include <oblo/asset/importers/stb_image.hpp>

namespace oblo::importers
{
    namespace
    {
        template <typename T>
        file_importer_descriptor make_file_importer_desc(std::span<const string_view> extensions)
        {
            return file_importer_descriptor{
                .type = get_type_id<T>(),
                .create = []() -> unique_ptr<file_importer> { return allocate_unique<T>(); },
                .extensions = extensions,
            };
        }

        constexpr string_view g_gltfExtensions[] = {".gltf", ".glb"};
        constexpr string_view g_stbExtensions[] = {".jpg", ".jpeg", ".png", ".tga", ".bmp", ".hdr"};
        constexpr string_view g_ddsExtensions[] = {".dds"};
        constexpr string_view g_assimpExtensions[] = {".fbx"};
    }

    void register_gltf_importer(asset_registry& registry)
    {
        registry.register_file_importer(make_file_importer_desc<gltf>(g_gltfExtensions));
    }

    void unregister_gltf_importer(asset_registry& registry)
    {
        registry.unregister_file_importer(get_type_id<gltf>());
    }

    void register_stb_image_importer(asset_registry& registry)
    {
        registry.register_file_importer(make_file_importer_desc<stb_image>(g_stbExtensions));
    }

    void unregister_stb_image_importer(asset_registry& registry)
    {
        registry.unregister_file_importer(get_type_id<stb_image>());
    }

    void fetch_importers(dynamic_array<file_importer_descriptor>& outResourceTypes)
    {
        outResourceTypes.emplace_back(make_file_importer_desc<gltf>(g_gltfExtensions));
        outResourceTypes.emplace_back(make_file_importer_desc<stb_image>(g_stbExtensions));
        outResourceTypes.emplace_back(make_file_importer_desc<dds>(g_ddsExtensions));
        outResourceTypes.emplace_back(make_file_importer_desc<assimp>(g_assimpExtensions));
    }
}