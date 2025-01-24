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

            constexpr f32 rowHeight = 28.f;

            if (m)
            {
                bool modified = false;

                string_builder builder;

                if (ImGui::BeginTable("#logs",
                        2,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto& property : m->get_properties())
                    {
                        builder.clear().format("{}", property.name);

                        ImGui::PushID(std::bit_cast<void*>(property.name.hash()));

                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);

                        ImGui::Dummy({0, rowHeight});
                        ImGui::SameLine();

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (rowHeight - ImGui::GetTextLineHeight()) * .5f);

                        ImGui::TextUnformatted(builder.c_str());

                        ImGui::TableSetColumnIndex(1);

                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

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
                                    modified |= ImGui::DragFloat("", &*r);
                                    isCorrectType = true;
                                }
                            }
                            break;

                            case material_property_type::linear_color_rgb_f32: {
                                auto r = property.as<vec3>();

                                if (r)
                                {
                                    modified |= ImGui::ColorEdit3("", &r->x);
                                    isCorrectType = true;
                                }
                            }
                            break;

                            default:
                                break;
                            }

                            // TODO: Maybe still handle if the type is not correct?

                            ImGui::PopID();
                        }
                    }

                    ImGui::EndTable();

                    if (modified)
                    {
                        // TODO: Should save here
                    }
                }
            }
        }

        ImGui::End();

        ImGui::PopID();

        return isOpen;
    }
}