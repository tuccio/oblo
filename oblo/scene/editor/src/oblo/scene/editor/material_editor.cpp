#include <oblo/scene/editor/material_editor.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/incremental_id_pool.hpp>
#include <oblo/editor/ui/artifact_picker.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>
#include <oblo/scene/resources/traits.hpp>

#include <imgui.h>

namespace oblo::editor
{
    material_editor::material_editor(uuid assetId) : m_assetId{assetId} {}

    material_editor::~material_editor()
    {
        if (m_idPool)
        {
            m_idPool->release<material_editor>(m_id);
        }
    }

    bool material_editor::init(const window_update_context& ctx)
    {
        m_assetRegistry = ctx.services.find<asset_registry>();

        if (!m_assetRegistry)
        {
            return false;
        }

        m_idPool = ctx.services.find<incremental_id_pool>();

        if (!m_idPool)
        {
            return false;
        }

        m_id = m_idPool->acquire<material_editor>();

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

        pbr::properties pbr;

        auto addPropertyDesc = [this](const material_property_descriptor& desc)
        { m_propertyEditor.emplace(desc.name, desc); };

        struct_apply([&](auto&&... descs) { (addPropertyDesc(descs), ...); }, pbr);

        m_artifactPicker = allocate_unique<ui::artifact_picker>(*m_assetRegistry);

        return true;
    }

    bool material_editor::update(const window_update_context&)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

        bool isOpen{true};

        string_builder builder;
        builder.format("Material Editor##{}", uintptr(this));

        if (ImGui::Begin(builder.c_str(), &isOpen))
        {
            auto* const m = m_asset.as<material>();

            if (m && ui::property_table::begin())
            {
                bool modified = false;

                for (const auto& property : m->get_properties())
                {
                    builder.clear().format("{}", property.name);
                    const cstring_view propertyName{builder};
                    const auto id = ImGui::GetID(property.name.begin(), property.name.end());

                    const auto propertyIt = m_propertyEditor.find(property.name);

                    bool isCorrectType = false;

                    if (propertyIt != m_propertyEditor.end())
                    {
                        switch (propertyIt->second.type)
                        {
                        case material_property_type::f32: {

                            auto r = property.as<f32>();

                            if (r)
                            {
                                if (ui::property_table::add(id, propertyName, *r))
                                {
                                    modified = true;
                                    m->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::linear_color_rgb_f32: {
                            auto r = property.as<vec3>();

                            if (r)
                            {
                                if (ui::property_table::add_color(id, propertyName, *r))
                                {
                                    modified = true;
                                    m->set_property<material_type_tag::linear_color>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::vec2: {
                            auto r = property.as<vec2>();

                            if (r)
                            {
                                if (ui::property_table::add(id, propertyName, *r))
                                {
                                    modified = true;
                                    m->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        case material_property_type::vec3: {
                            auto r = property.as<vec3>();

                            if (r)
                            {
                                if (ui::property_table::add(id, propertyName, *r))
                                {
                                    modified = true;
                                    m->set_property<material_type_tag::none>(property.name, *r);
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
                                        resource_type<texture>))
                                {
                                    modified = true;
                                    m->set_property<material_type_tag::none>(property.name, *r);
                                }

                                isCorrectType = true;
                            }
                        }
                        break;

                        default:
                            break;
                        }
                    }
                }

                ui::property_table::end();

                if (modified)
                {
                    if (!m_assetRegistry->save_asset(m_asset, m_assetId))
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
}