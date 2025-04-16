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
    expected<> scene_editor::open(window_manager& wm, asset_registry& assetRegistry, window_handle parent, uuid assetId)
    {
        auto anyAsset = assetRegistry.load_asset(assetId);

        if (!anyAsset)
        {
            return unspecified_error;
        }

        auto* const sceneAsset = anyAsset->as<scene>();

        if (!sceneAsset)
        {
            return unspecified_error;
        }

        const auto h = wm.create_child_window<scene_editing_window>(parent, {}, service_registry{});

        if (!h)
        {
            return unspecified_error;
        }

        m_editor = h;
        m_assetId = assetId;

        auto* const sceneEditor = wm.try_access<scene_editing_window>(m_editor);

        if (!sceneEditor)
        {
            wm.destroy_window(h);
            return unspecified_error;
        }

        if (!m_serializationContext.init() ||
            // We copy the asset 1:1 here, including transient types and entities if present
            !sceneAsset->copy_to(sceneEditor->get_entity_registry(),
                m_serializationContext.get_property_registry(),
                {},
                {}))
        {
            wm.destroy_window(h);
            return unspecified_error;
        }

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

        if (!sceneAsset.init(m_serializationContext.get_type_registry()))
        {
            return unspecified_error;
        }

        if (!sceneAsset.copy_from(entityRegistry,
                m_serializationContext.get_property_registry(),
                m_serializationContext.make_write_config(),
                m_serializationContext.make_read_config()))
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