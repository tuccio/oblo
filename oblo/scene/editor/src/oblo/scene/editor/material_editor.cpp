#include <oblo/scene/editor/material_editor.hpp>

#include <oblo/asset/any_asset.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/incremental_id_pool.hpp>
#include <oblo/editor/ui/artifact_picker.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/traits.hpp>

#include <imgui.h>

namespace oblo::editor
{
    class material_editor_window final
    {
    public:
        material_editor_window(asset_registry& assetRegistry, uuid assetId);
        material_editor_window(const resource_registry& resourceRegistry, uuid resourceId);

        ~material_editor_window();

        bool init(const window_update_context& ctx);
        bool update(const window_update_context& ctx);

        expected<> save_asset(asset_registry& assetRegistry) const;

    private:
        asset_registry* m_assetRegistry{};
        const resource_registry* m_resourceRegistry{};
        incremental_id_pool* m_idPool{};
        u32 m_id{};
        uuid m_assetId{};
        resource_ref<material> m_resourceRef{};
        any_asset m_asset;
        resource_ptr<material> m_resource;
        std::unordered_map<hashed_string_view, material_property_descriptor, hash<hashed_string_view>> m_propertyEditor;
        unique_ptr<ui::artifact_picker> m_artifactPicker;
        bool m_isResourceOnly{};
    };

    material_editor_window::material_editor_window(asset_registry& assetRegistry, uuid assetId) :
        m_assetRegistry{&assetRegistry}, m_assetId{assetId}, m_isResourceOnly{false}
    {
    }

    material_editor_window::material_editor_window(const resource_registry& resourceRegistry, uuid resourceId) :
        m_resourceRegistry{&resourceRegistry}, m_resourceRef{resourceId}, m_isResourceOnly{true}
    {
    }

    material_editor_window::~material_editor_window()
    {
        if (m_idPool)
        {
            m_idPool->release<material_editor_window>(m_id);
        }
    }

    bool material_editor_window::init(const window_update_context& ctx)
    {
        m_idPool = ctx.services.find<incremental_id_pool>();

        if (!m_idPool)
        {
            return false;
        }

        m_id = m_idPool->acquire<material_editor_window>();

        if (!m_isResourceOnly)
        {
            auto asset = m_assetRegistry->load_asset(m_assetId);

            if (!asset)
            {
                return false;
            }

            m_asset = std::move(*asset);

            if (!m_asset.is<material>())
            {
                return false;
            }
        }
        else
        {
            m_resource = m_resourceRegistry->get_resource(m_resourceRef);

            if (!m_resource)
            {
                return false;
            }

            m_resource.load_start_async();
        }

        pbr::properties pbr;

        auto addPropertyDesc = [this](const material_property_descriptor& desc)
        { m_propertyEditor.emplace(desc.name, desc); };

        struct_apply([&](auto&&... descs) { (addPropertyDesc(descs), ...); }, pbr);

        if (m_assetRegistry)
        {
            m_artifactPicker = allocate_unique<ui::artifact_picker>(*m_assetRegistry);
        }
        else
        {
            m_artifactPicker = allocate_unique<ui::artifact_picker>(*m_resourceRegistry);
        }

        return true;
    }

    bool material_editor_window::update(const window_update_context&)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

        bool isOpen{true};

        string_builder builder;
        builder.format("Material Editor##{}", m_id);

        if (ImGui::Begin(builder.c_str(), &isOpen))
        {
            const material* readOnlyMaterial{};
            material* mutableMaterial{};

            if (m_isResourceOnly)
            {
                readOnlyMaterial = m_resource.get();

                if (!readOnlyMaterial && !m_resource.is_nor_loaded_or_loading())
                {
                    m_resource.load_start_async();
                }
            }
            else
            {
                mutableMaterial = m_asset.as<material>();
                readOnlyMaterial = mutableMaterial;
            }

            if (readOnlyMaterial && ui::property_table::begin())
            {
                if (!mutableMaterial)
                {
                    ImGui::BeginDisabled();
                }

                bool modified = false;

                for (const auto& property : readOnlyMaterial->get_properties())
                {
                    builder.clear().format("{}", property.name);
                    const cstring_view propertyName{builder};
                    const auto id = ImGui::GetID(property.name.begin(), property.name.end());

                    const auto propertyIt = m_propertyEditor.find(property.name);

                    [[maybe_unused]] bool isCorrectType = false;

                    if (propertyIt != m_propertyEditor.end())
                    {
                        switch (propertyIt->second.type)
                        {
                        case material_property_type::f32: {

                            auto r = property.as<f32>();

                            if (r)
                            {
                                if (ui::property_table::add(id, propertyName, *r) && mutableMaterial)
                                {
                                    modified = true;
                                    mutableMaterial->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::linear_color_rgb_f32: {
                            auto r = property.as<vec3>();

                            if (r)
                            {
                                if (ui::property_table::add_color(id, propertyName, *r) && mutableMaterial)
                                {
                                    modified = true;
                                    mutableMaterial->set_property<material_type_tag::linear_color>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::vec2: {
                            auto r = property.as<vec2>();

                            if (r)
                            {
                                if (ui::property_table::add(id, propertyName, *r) && mutableMaterial)
                                {
                                    modified = true;
                                    mutableMaterial->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::vec3: {
                            auto r = property.as<vec3>();

                            if (r)
                            {
                                if (ui::property_table::add(id, propertyName, *r) && mutableMaterial)
                                {
                                    modified = true;
                                    mutableMaterial->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::texture: {
                            auto r = property.as<resource_ref<texture>>();

                            if (r)
                            {
                                if (ui::property_table::add(id,
                                        propertyName,
                                        r->id,
                                        *m_artifactPicker,
                                        resource_type<texture>) &&
                                    mutableMaterial)
                                {
                                    modified = true;
                                    mutableMaterial->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        default:
                            break;
                        }

                        OBLO_ASSERT(isCorrectType);
                    }
                }

                ui::property_table::end();

                if (!mutableMaterial)
                {
                    ImGui::EndDisabled();
                }

                if (modified)
                {
                    if (!save_asset(*m_assetRegistry))
                    {
                        log::error("Failed to save material {}", m_assetId);
                    }
                }
            }
        }

        ImGui::End();

        ImGui::PopStyleVar(2);

        return isOpen;
    }

    expected<> material_editor_window::save_asset(asset_registry& assetRegistry) const
    {
        return assetRegistry.save_asset(m_asset, m_assetId);
    }

    // Material editor

    expected<> material_editor::open(
        window_manager& wm, asset_registry& assetRegistry, window_handle parent, uuid assetId)
    {
        const auto h = wm.create_child_window<material_editor_window>(parent, {}, {}, assetRegistry, assetId);

        if (!h)
        {
            return "Editor operation failed"_err;
        }

        m_editor = h;

        return no_error;
    }

    void material_editor::close(window_manager& wm)
    {
        wm.destroy_window(m_editor);
        m_editor = {};
    }

    expected<> material_editor::save(window_manager& wm, asset_registry& assetRegistry)
    {
        auto* const materialEditor = wm.try_access<material_editor_window>(m_editor);

        if (!materialEditor)
        {
            return "Editor operation failed"_err;
        }

        return materialEditor->save_asset(assetRegistry);
    }

    window_handle material_editor::get_window() const
    {
        return m_editor;
    }

    // Material viewer

    expected<> material_viewer::open(
        window_manager& wm, const resource_registry& resourceRegistry, window_handle parent, uuid resourceId)
    {
        const auto h = wm.create_child_window<material_editor_window>(parent, {}, {}, resourceRegistry, resourceId);

        if (!h)
        {
            return "Editor operation failed"_err;
        }

        m_editor = h;

        return no_error;
    }

    void material_viewer::close(window_manager& wm)
    {
        wm.destroy_window(m_editor);
        m_editor = {};
    }

    window_handle material_viewer::get_window() const
    {
        return m_editor;
    }
}