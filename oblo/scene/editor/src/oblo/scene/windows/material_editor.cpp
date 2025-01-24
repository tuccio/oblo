#include <oblo/scene/windows/material_editor.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/pbr_properties.hpp>

#include <imgui.h>

namespace oblo::editor
{
    material_editor::material_editor(uuid assetId) : m_assetId{assetId} {}

    material_editor::~material_editor() = default;

    bool material_editor::init(const window_update_context& ctx)
    {
        m_assetRegistry = ctx.services.find<asset_registry>();

        if (!m_assetRegistry)
        {
            return false;
        }

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

        return true;
    }

    bool material_editor::update(const window_update_context&)
    {
        ImGui::PushID(this);

        bool isOpen{true};

        if (ImGui::Begin("Material Editor", &isOpen))
        {
            auto* const m = m_asset.as<material>();

            if (m)
            {
                string_builder builder;

                // TODO: Draw material editor
                for (const auto& property : m->get_properties())
                {
                    builder.clear().format("{}", property.name);
                    ImGui::TextUnformatted(builder.c_str());

                    const auto propertyIt = m_propertyEditor.find(property.name);

                    if (propertyIt != m_propertyEditor.end())
                    {
                        switch (propertyIt->second.type)
                        {
                        case material_property_type::linear_color_rgb_f32: {
                            auto r = property.as<vec3>();

                            if (r)
                            {
                                ImGui::ColorEdit3("", &r->x);
                                continue;
                            }
                        }
                        default:
                            break;
                        }

                        // TODO: Maybe still handle ?
                    }
                }
            }
        }

        ImGui::End();

        ImGui::PopID();

        return isOpen;
    }
}