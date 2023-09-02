#include <oblo/asset/asset_registry.hpp>

#include <oblo/asset/asset_type_desc.hpp>
#include <oblo/scene/assets/bundle.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>

namespace oblo::scene
{
    template <typename T>
    asset::asset_type_desc make_asset_type_desc()
    {
        return {
            .type = get_type_id<T>(),
            .create = []() -> void* { return new T{}; },
            .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
            .load =
                [](void*, const std::filesystem::path&)
            {
                // TODO?
            },
            .save =
                [](const void*, const std::filesystem::path&)
            {
                // TODO
            },
        };
    }
    void register_asset_types(asset::asset_registry& registry)
    {
        registry.register_type(make_asset_type_desc<mesh>());
        registry.register_type(make_asset_type_desc<model>());
        registry.register_type(make_asset_type_desc<bundle>());
    }
}