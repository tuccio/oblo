#include <oblo/editor/ui/artifact_picker.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>

#include <IconsFontAwesome6.h>

#include <imgui.h>

namespace oblo::editor::ui
{
    namespace
    {
        void make_artifact_name(
            string_builder& builder, const uuid& id, artifact_meta& meta, const asset_registry& registry)
        {
            if (id.is_nil())
            {
                builder.append("None");
            }
            else if (registry.find_artifact_by_id(id, meta))
            {
                if (!registry.get_asset_name(meta.assetId, builder))
                {
                    OBLO_ASSERT(false);
                    builder.format("Unknown Asset");
                }

                builder.format("/{}", meta.name);
            }
            else
            {
                builder.format("{}", id);
            }
        }
    }

    artifact_picker::artifact_picker(asset_registry& registry) : m_assetRegistry{registry} {}

    bool artifact_picker::draw(int uiId, const uuid& type, const uuid& ref)
    {
        m_currentRef = ref;

        ImGui::PushID(uiId);

        artifact_meta meta;

        string_builder builder;
        make_artifact_name(builder, m_currentRef, meta, m_assetRegistry);

        bool selectionChanged{};

        if (ImGui::BeginCombo("", builder.c_str()))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            if (ImGui::InputTextWithHint("##search",
                    "Filter ... " ICON_FA_MAGNIFYING_GLASS,
                    m_textFilter.InputBuf,
                    sizeof(m_textFilter.InputBuf)))
            {
                m_textFilter.Build();
            }

            m_assetRegistry.iterate_artifacts_by_type(type,
                [this, &builder, &selectionChanged, &meta](const uuid&, const uuid& artifactId)
                {
                    builder.clear();

                    make_artifact_name(builder, artifactId, meta, m_assetRegistry);

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

            ImGui::PopStyleVar();

            ImGui::EndCombo();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (auto* const artifactPayload = ImGui::AcceptDragDropPayload(payloads::Artifact))
            {
                const uuid id = payloads::unpack_artifact(artifactPayload->Data);

                artifact_meta dndMeta;

                if (m_assetRegistry.find_artifact_by_id(id, dndMeta) && meta.type == type)
                {
                    m_currentRef = id;
                    selectionChanged = true;
                }
            }
            else if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Asset))
            {
                const uuid id = payloads::unpack_asset(assetPayload->Data);

                asset_meta assetMeta;
                artifact_meta artifactMeta;

                if (m_assetRegistry.find_asset_by_id(id, assetMeta) &&
                    m_assetRegistry.find_artifact_by_id(assetMeta.mainArtifactHint, artifactMeta) && meta.type == type)
                {
                    m_currentRef = artifactMeta.artifactId;
                    selectionChanged = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();

        return selectionChanged;
    }

    uuid artifact_picker::get_current_ref() const
    {
        return m_currentRef;
    }
}