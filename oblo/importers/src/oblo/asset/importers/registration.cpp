#include <oblo/asset/importers/registration.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importers/gltf.hpp>
#include <oblo/asset/importers/stb_image.hpp>

namespace oblo::importers
{
    namespace
    {
        template <typename T>
        file_importer_desc make_file_importer_desc(std::span<const std::string_view> extensions)
        {
            return file_importer_desc{
                .type = get_type_id<T>(),
                .create = []() -> std::unique_ptr<file_importer> { return std::make_unique<T>(); },
                .extensions = extensions,
            };
        }
    }

    void register_gltf_importer(asset_registry& registry)
    {
        constexpr std::string_view extensions[] = {".gltf", ".glb"};
        registry.register_file_importer(make_file_importer_desc<gltf>(extensions));
    }

    void unregister_gltf_importer(asset_registry& registry)
    {
        registry.unregister_file_importer(get_type_id<gltf>());
    }

    void register_stb_image_importer(asset_registry& registry)
    {
        constexpr std::string_view extensions[] = {".jpg", ".jpeg", ".png", ".tga", ".bmp"};
        registry.register_file_importer(make_file_importer_desc<stb_image>(extensions));
    }

    void unregister_stb_image_importer(asset_registry& registry)
    {
        registry.unregister_file_importer(get_type_id<stb_image>());
    }
}