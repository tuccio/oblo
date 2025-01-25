#include <oblo/editor/ui/artifact_picker.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/string_builder.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>

namespace oblo::editor::ui
{
    artifact_picker::artifact_picker(asset_registry& registry) : m_assetRegistry{registry} {}

    bool artifact_picker::draw(int uiId, const uuid& type, const uuid& ref)
    {
        m_currentRef = ref;

        ImGui::PushID(uiId);

        string_builder builder;
        builder.format("{}", m_currentRef);

        bool selectionChanged{};

        if (ImGui::BeginCombo("", builder.c_str()))
        {
            if (ImGui::InputTextWithHint("##search",
                    "Filter ... " ICON_FA_MAGNIFYING_GLASS,
                    m_textFilter.InputBuf,
                    sizeof(m_textFilter.InputBuf)))
            {
                m_textFilter.Build();
            }

            m_assetRegistry.iterate_artifacts_by_type(type,
                [this, &builder, &selectionChanged](const uuid&, const uuid& artifactId)
                {
                    builder.clear();

                    artifact_meta meta;

                    if (m_assetRegistry.load_artifact_meta(artifactId, meta))
                    {
                        builder.format("{}", meta.importName);
                    }
                    else
                    {
                        builder.format("{}", artifactId);
                    }

                    if (!m_textFilter.PassFilter(builder.begin(), builder.end()))
                    {
                        return true;
                    }

                    if (ImGui::Selectable(builder.c_str()))
                    {
                        selectionChanged = true;
                        m_currentRef = artifactId;
                    }

                    return true;
                });

            ImGui::EndCombo();
        }

        ImGui::PopID();

        return selectionChanged;
    }

    uuid artifact_picker::get_current_ref() const
    {
        return m_currentRef;
    }
}