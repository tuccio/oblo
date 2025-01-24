#include <oblo/editor/windows/asset_browser.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/formatters/uuid_formatter.hpp>
#include <oblo/core/platform/shell.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/providers/asset_editor_provider.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_manager.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/log/log.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <imgui.h>

#include <filesystem>
#include <unordered_map>

namespace oblo::editor
{
    namespace
    {
        struct create_menu_item
        {
            cstring_view name;
            asset_create_fn create{};
        };
    }

    struct asset_browser::impl
    {
        asset_registry* registry{};
        string_builder path;
        string_builder current;
        uuid expandedAsset{};
        dynamic_array<string> breadcrumbs;
        deque<create_menu_item> createMenu;
        std::unordered_map<uuid, asset_editor_create_fn> editorsLookup;

        void populate_asset_editors();
        void draw_popup_menu();
        void reset_path();
    };

    asset_browser::asset_browser() = default;
    asset_browser::~asset_browser() = default;

    void asset_browser::init(const window_update_context& ctx)
    {
        m_impl = allocate_unique<impl>();

        m_impl->registry = ctx.services.find<asset_registry>();
        OBLO_ASSERT(m_impl->registry);

        m_impl->path = m_impl->registry->get_asset_directory();
        m_impl->path.make_canonical_path();

        m_impl->current = m_impl->path;

        m_impl->populate_asset_editors();
    }

    bool asset_browser::update(const window_update_context& ctx)
    {
        bool open{true};

        if (ImGui::Begin("Asset Browser", &open))
        {
            m_impl->draw_popup_menu();

            if (m_impl->current != m_impl->path)
            {
                if (ImGui::Button(".."))
                {
                    std::error_code ec;
                    m_impl->current.append_path("..").make_canonical_path();
                    m_impl->breadcrumbs.pop_back();

                    if (ec)
                    {
                        m_impl->reset_path();
                    }
                }
            }

            std::error_code ec;

            string_builder directoryName;

            for (auto&& entry : std::filesystem::directory_iterator{m_impl->current.as<std::string>(), ec})
            {
                const auto& p = entry.path();
                if (std::filesystem::is_directory(p))
                {
                    auto dir = p.filename();
                    directoryName.clear().append(dir.native().c_str());

                    if (ImGui::Button(directoryName.c_str()))
                    {
                        m_impl->current.clear().append(p.native().c_str()).make_canonical_path();
                        m_impl->breadcrumbs.emplace_back(directoryName.as<string>());
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

                    if (m_impl->registry->find_asset_by_meta_path(assetPath, assetId, meta))
                    {
                        ImGui::PushID(int(hash_all<std::hash>(assetId)));

                        if (ImGui::Button(reinterpret_cast<const char*>(str.c_str())))
                        {
                            if (const auto it = m_impl->editorsLookup.find(meta.typeHint);
                                it != m_impl->editorsLookup.end())
                            {
                                const asset_editor_create_fn createWindow = it->second;
                                createWindow(ctx.windowManager,
                                    ctx.windowManager.get_parent(ctx.windowHandle),
                                    assetId);
                            }
                            else
                            {
                                m_impl->expandedAsset == meta.assetId ? oblo::uuid{} : meta.assetId;
                            }
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
                                if (!m_impl->registry->process(assetId))
                                {
                                    log::error("Failed to reimport {}", assetId);
                                }
                            }

                            if (ImGui::MenuItem("Open Source in Explorer"))
                            {
                                string_builder sourcePath;
                                if (m_impl->registry->get_source_directory(assetId, sourcePath))
                                {
                                    platform::open_folder(sourcePath.view());
                                }
                            }

                            ImGui::EndPopup();
                        }

                        ImGui::PopID();

                        if (m_impl->expandedAsset == meta.assetId)
                        {
                            dynamic_array<oblo::uuid> artifacts;

                            if (m_impl->registry->find_asset_artifacts(meta.assetId, artifacts))
                            {
                                for (const auto& artifact : artifacts)
                                {
                                    artifact_meta artifactMeta;

                                    if (m_impl->registry->load_artifact_meta(artifact, artifactMeta))
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
                m_impl->reset_path();
            }

            ImGui::End();
        }

        return open;
    }

    void asset_browser::impl::reset_path()
    {
        current = path;
        breadcrumbs.clear();
    }

    void asset_browser::impl::populate_asset_editors()
    {
        auto& mm = module_manager::get();

        deque<asset_editor_descriptor> createDescs;

        for (auto* const createProvider : mm.find_services<asset_editor_provider>())
        {
            createDescs.clear();
            createProvider->fetch(createDescs);

            for (const auto& desc : createDescs)
            {
                editorsLookup.emplace(desc.assetType, desc.openEditorWindow);

                // Ignoring category for now
                createMenu.emplace_back(desc.name, desc.create);
            }
        }
    }

    void asset_browser::impl::draw_popup_menu()
    {
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Import"))
            {
                string_builder file;

                if (platform::open_file_dialog(file))
                {
                    const auto r = registry->import(file, current, data_document{});

                    if (!r)
                    {
                        log::error("No importer was found for {}", file);
                    }
                }
            }

            if (!createMenu.empty() && ImGui::BeginMenu("Create"))
            {
                for (const auto& item : createMenu)
                {
                    if (ImGui::MenuItem(item.name.c_str()))
                    {
                        // TODO: Need to find a suitable and available name instead
                        const auto r = registry->create_asset(item.create(), current, "New Asset");

                        if (!r)
                        {
                            log::error("Failed to create new asset {}", item.name);
                        }
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Open in Explorer"))
            {
                platform::open_folder(current.view());
            }

            ImGui::EndPopup();
        }
    }
}