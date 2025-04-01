#include <oblo/scene/editor/scene_editor.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/scene/assets/scene.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/components/tags.hpp>
#include <oblo/scene/editor/scene_editing_window.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>

namespace oblo::editor
{
    expected<> scene_editor::open(window_manager& wm, window_handle parent, uuid assetId)
    {
        const auto h = wm.create_child_window<scene_editing_window>(parent, {}, service_registry{});

        if (!h)
        {
            return unspecified_error;
        }

        m_editor = h;
        m_assetId = assetId;

        return no_error;
    }

    void scene_editor::close(window_manager& wm)
    {
        wm.destroy_window(m_editor);
        m_editor = {};
    }

    expected<> scene_editor::save(window_manager& wm, asset_registry& assetRegistry)
    {
        auto* const sceneEditor = wm.try_access<scene_editing_window>(m_editor);

        if (!sceneEditor)
        {
            return unspecified_error;
        }

        any_asset asset;
        auto& sceneAsset = asset.emplace<scene>();

        const auto& entityRegistry = sceneEditor->get_entity_registry();

        if (!sceneAsset.init())
        {
            return unspecified_error;
        }

        if (!sceneAsset.copy_from(entityRegistry,
                {
                    .skipEntities = ecs::make_type_sets<transient_tag>(entityRegistry.get_type_registry()),
                }))
        {
            return unspecified_error;
        }

        return assetRegistry.save_asset(asset, m_assetId);
    }

    window_handle scene_editor::get_window() const
    {
        return m_editor;
    }
}