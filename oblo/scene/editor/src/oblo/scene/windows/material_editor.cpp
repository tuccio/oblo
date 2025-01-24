#include <oblo/scene/windows/material_editor.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/scene/assets/traits.hpp>
#include <oblo/scene/resources/material.hpp>

#include <imgui.h>

namespace oblo::editor
{
    material_editor::material_editor(uuid assetId) : m_assetId{assetId} {}

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
        return m_asset.is<material>();
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
                }
            }
        }

        ImGui::End();

        ImGui::PopID();

        return isOpen;
    }
}