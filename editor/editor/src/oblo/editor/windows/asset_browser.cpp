#include <oblo/editor/windows/asset_browser.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/platform/shell.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/log/log.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <imgui.h>

#include <filesystem>

namespace oblo::editor
{
    void asset_browser::init(const window_update_context& ctx)
    {
        m_registry = ctx.services.find<asset_registry>();
        OBLO_ASSERT(m_registry);

        m_path = m_registry->get_asset_directory();
        m_path.make_canonical_path();

        m_current = m_path;
    }

    bool asset_browser::update(const window_update_context&)
    {
        bool open{true};

        if (ImGui::Begin("Asset Browser", &open))
        {
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::MenuItem("Import"))
                {
                    string_builder file;

                    if (platform::open_file_dialog(file))
                    {
                        const auto r = m_registry->import(file, m_current, data_document{});

                        if (!r)
                        {
                            log::error("No importer was found for {}", file);
                        }
                    }
                }

                if (ImGui::MenuItem("Open in Explorer"))
                {
                    platform::open_folder(m_current.view());
                }

                ImGui::EndPopup();
            }

            if (m_current != m_path)
            {
                if (ImGui::Button(".."))
                {
                    std::error_code ec;
                    m_current.append_path("..").make_canonical_path();
                    m_breadcrumbs.pop_back();

                    if (ec)
                    {
                        reset_path();
                    }
                }
            }

            std::error_code ec;

            string_builder directoryName;

            for (auto&& entry : std::filesystem::directory_iterator{m_current.as<std::string>(), ec})
            {
                const auto& p = entry.path();
                if (std::filesystem::is_directory(p))
                {
                    auto dir = p.filename();
                    directoryName.clear().append(dir.native().c_str());

                    if (ImGui::Button(directoryName.c_str()))
                    {
                        m_current.clear().append(p.native().c_str()).make_canonical_path();
                        m_breadcrumbs.emplace_back(directoryName.as<string>());
                    }
                }
                else if (p.extension() == AssetMetaExtension.c_str())
                {
                    const auto file = p.filename();
                    const auto& str = file.u8string();

                    uuid assetId;
                    asset_meta meta;

                    string_builder assetPath;
                    assetPath.append(p.native().c_str());

                    if (m_registry->find_asset_by_meta_path(assetPath, assetId, meta))
                    {
                        ImGui::PushID(int(hash_all<std::hash>(assetId)));

                        if (ImGui::Button(reinterpret_cast<const char*>(str.c_str())))
                        {
                            m_expandedAsset = m_expandedAsset == meta.assetId ? oblo::uuid{} : meta.assetId;
                        }
                        else if (!meta.mainArtifactHint.is_nil() && ImGui::BeginDragDropSource())
                        {
                            const auto payload = payloads::pack_uuid(meta.mainArtifactHint);
                            ImGui::SetDragDropPayload(payloads::Resource, &payload, sizeof(drag_and_drop_payload));
                            ImGui::EndDragDropSource();
                        }

                        if (ImGui::BeginPopupContextItem("##assetctx"))
                        {
                            if (ImGui::MenuItem("Reimport"))
                            {
                                if (!m_registry->process(assetId))
                                {
                                    log::error("Failed to reimport {}", assetId);
                                }
                            }

                            ImGui::EndPopup();
                        }

                        ImGui::PopID();

                        if (m_expandedAsset == meta.assetId)
                        {
                            dynamic_array<oblo::uuid> artifacts;

                            if (m_registry->find_asset_artifacts(meta.assetId, artifacts))
                            {
                                for (const auto& artifact : artifacts)
                                {
                                    artifact_meta artifactMeta;

                                    if (m_registry->load_artifact_meta(artifact, artifactMeta))
                                    {
                                        ImGui::PushID(int(hash_all<std::hash>(artifact)));

                                        ImGui::Button(artifactMeta.importName.empty()
                                                ? "Unnamed Artifact"
                                                : artifactMeta.importName.c_str());

                                        ImGui::PopID();

                                        if (ImGui::BeginDragDropSource())
                                        {
                                            const auto payload = payloads::pack_uuid(artifact);

                                            ImGui::SetDragDropPayload(payloads::Resource,
                                                &payload,
                                                sizeof(drag_and_drop_payload));

                                            ImGui::EndDragDropSource();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (ec)
            {
                reset_path();
            }

            ImGui::End();
        }

        return open;
    }

    void asset_browser::reset_path()
    {
        m_current = m_path;
        m_breadcrumbs.clear();
    }
}