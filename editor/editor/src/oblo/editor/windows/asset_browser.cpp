#include <oblo/editor/windows/asset_browser.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/platform/shell.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>

#include <imgui.h>

namespace oblo::editor
{
    void asset_browser::init(const window_update_context& ctx)
    {
        m_registry = ctx.services.find<asset_registry>();
        OBLO_ASSERT(m_registry);

        m_path = std::filesystem::canonical(m_registry->get_asset_directory());
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
                    std::filesystem::path file;

                    if (platform::open_file_dialog(file))
                    {
                        auto importer = m_registry->create_importer(file);

                        if (importer.is_valid() && importer.init())
                        {
                            importer.execute(m_current);
                        }
                    }
                }

                if (ImGui::MenuItem("Open in Explorer"))
                {
                    platform::open_folder(m_current);
                }

                ImGui::EndPopup();
            }

            if (m_current != m_path)
            {
                if (ImGui::Button(".."))
                {
                    std::error_code ec;
                    m_current = std::filesystem::canonical(m_current / "..", ec);
                    m_breadcrumbs.pop_back();

                    if (ec)
                    {
                        reset_path();
                    }
                }
            }

            std::error_code ec;

            for (auto&& entry : std::filesystem::directory_iterator{m_current, ec})
            {
                const auto& p = entry.path();
                if (std::filesystem::is_directory(p))
                {
                    auto dir = p.filename();
                    const auto& str = dir.u8string();

                    if (ImGui::Button(reinterpret_cast<const char*>(str.c_str())))
                    {
                        m_current = std::filesystem::canonical(p, ec);
                        m_breadcrumbs.emplace_back(std::move(dir));
                    }
                }
                else if (p.native().ends_with(AssetMetaExtension))
                {
                    const auto file = p.filename();
                    const auto& str = file.u8string();

                    uuid uuid;
                    asset_meta meta;

                    if (m_registry->find_asset_by_meta_path(p, uuid, meta))
                    {
                        ImGui::PushID(int(hash_all<std::hash>(uuid)));

                        if (ImGui::Button(reinterpret_cast<const char*>(str.c_str())))
                        {
                            m_expandedAsset = m_expandedAsset == meta.id ? oblo::uuid{} : meta.id;
                        }
                        else if (!meta.mainArtifactHint.is_nil() && ImGui::BeginDragDropSource())
                        {
                            const auto payload = payloads::pack_uuid(meta.mainArtifactHint);
                            ImGui::SetDragDropPayload(payloads::Resource, &payload, sizeof(drag_and_drop_payload));
                            ImGui::EndDragDropSource();
                        }

                        ImGui::PopID();

                        if (m_expandedAsset == meta.id)
                        {
                            dynamic_array<oblo::uuid> artifacts;

                            if (m_registry->find_asset_artifacts(meta.id, artifacts))
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