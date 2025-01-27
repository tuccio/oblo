#include <oblo/editor/windows/asset_browser.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/filesystem/directory_watcher.hpp>
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
        enum class asset_browser_entry_kind : u8
        {
            asset,
            directory,
        };

        struct asset_browser_entry
        {
            asset_browser_entry_kind kind;
            asset_meta meta;
            deque<artifact_meta> artifacts;
            string_builder path;
            string_builder name;
        };

        struct asset_browser_directory
        {
            deque<asset_browser_entry> entries;
        };

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

        filesystem::directory_watcher assetDirWatcher;

        std::unordered_map<string_builder, asset_browser_directory, hash<string_builder>> assetBrowserEntries;
        std::unordered_map<uuid, asset_editor_create_fn> editorsLookup;

        u64 lastVersionId;

        void populate_asset_editors();
        void draw_popup_menu();
        void reset_path();
        asset_browser_directory& get_or_build(const string_builder& p);
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

        if (!m_impl->assetDirWatcher.init({
                .path = m_impl->path.view(),
                .isRecursive = true,
            }))
        {
            log::debug("Asset browser failed to start watch on '{}'", m_impl->path);
        }
    }

    bool asset_browser::update(const window_update_context& ctx)
    {
        bool open{true};

        if (ImGui::Begin("Asset Browser", &open))
        {
            bool requiresRefresh{};

            if (!m_impl->assetDirWatcher.process([&](auto&&) { requiresRefresh = true; }))
            {
                log::debug("Asset browser watch processing on '{}' failed", m_impl->path);
            }

            if (const auto vId = m_impl->registry->get_version_id(); requiresRefresh || m_impl->lastVersionId != vId)
            {
                m_impl->assetBrowserEntries.clear();
                m_impl->lastVersionId = vId;
            }

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

            const asset_browser_directory& dir = m_impl->get_or_build(m_impl->current);

            for (auto&& entry : dir.entries)
            {
                switch (entry.kind)
                {
                case asset_browser_entry_kind::directory:
                    if (ImGui::Button(entry.name.c_str()))
                    {
                        m_impl->current.clear().append(entry.path).make_canonical_path();
                        m_impl->breadcrumbs.emplace_back(entry.name.as<string>());
                    }

                    break;
                case asset_browser_entry_kind::asset: {
                    const asset_meta& meta = entry.meta;

                    ImGui::PushID(int(hash_all<std::hash>(meta.assetId)));

                    if (ImGui::Button(entry.name.c_str()))
                    {
                        if (const auto it = m_impl->editorsLookup.find(meta.typeHint);
                            it != m_impl->editorsLookup.end())
                        {
                            const asset_editor_create_fn createWindow = it->second;
                            createWindow(ctx.windowManager,
                                ctx.windowManager.get_parent(ctx.windowHandle),
                                meta.assetId);
                        }
                        else
                        {
                            m_impl->expandedAsset = m_impl->expandedAsset == meta.assetId ? uuid{} : meta.assetId;
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
                            if (!m_impl->registry->process(meta.assetId))
                            {
                                log::error("Failed to reimport {}", meta.assetId);
                            }
                        }

                        if (ImGui::MenuItem("Open Source in Explorer"))
                        {
                            string_builder sourcePath;
                            if (m_impl->registry->get_source_directory(meta.assetId, sourcePath))
                            {
                                platform::open_folder(sourcePath.view());
                            }
                        }

                        ImGui::EndPopup();
                    }

                    ImGui::PopID();

                    if (m_impl->expandedAsset == meta.assetId)
                    {
                        for (const auto& artifactMeta : entry.artifacts)
                        {
                            ImGui::PushID(int(hash_all<std::hash>(artifactMeta.artifactId)));

                            ImGui::Button(artifactMeta.name.empty() ? "Unnamed Artifact" : artifactMeta.name.c_str());

                            ImGui::PopID();

                            if (ImGui::BeginDragDropSource())
                            {
                                const auto payload = payloads::pack_uuid(artifactMeta.artifactId);

                                ImGui::SetDragDropPayload(payloads::Resource, &payload, sizeof(drag_and_drop_payload));

                                ImGui::EndDragDropSource();
                            }
                        }
                    }
                }
                break;
                }

                if (ec)
                {
                    m_impl->reset_path();
                }
            }
        }

        ImGui::End();

        return open;
    }

    void asset_browser::impl::reset_path()
    {
        current = path;
        breadcrumbs.clear();
    }

    asset_browser_directory& asset_browser::impl::get_or_build(const string_builder& assetDir)
    {
        std::error_code ec;

        const auto [it, isNew] = assetBrowserEntries.emplace(assetDir, asset_browser_directory{});
        auto& abDir = it->second;

        if (!isNew)
        {
            return abDir;
        }

        dynamic_array<uuid> artifacts;
        artifacts.reserve(128);

        for (auto&& fsEntry : std::filesystem::directory_iterator{assetDir.as<std::string>(), ec})
        {
            const auto& p = fsEntry.path();

            if (std::filesystem::is_directory(p))
            {
                auto name = p.filename();

                auto& e = abDir.entries.emplace_back();
                e.kind = asset_browser_entry_kind::directory;
                e.path.append(p.native().c_str());
                e.name.append(name.native().c_str());
            }
            else if (p.extension() == AssetMetaExtension.c_str())
            {
                auto name = p.filename();

                auto& e = abDir.entries.emplace_back();
                e.kind = asset_browser_entry_kind::asset;
                e.path.append(p.native().c_str());
                e.name.append(name.native().c_str());

                uuid assetId;

                if (!registry->find_asset_by_meta_path(e.path, assetId, e.meta))
                {
                    OBLO_ASSERT(false);
                    abDir.entries.pop_back();
                    continue;
                }

                if (registry->find_asset_artifacts(e.meta.assetId, artifacts))
                {
                    for (const auto& artifactId : artifacts)
                    {
                        auto& artifactEntry = e.artifacts.emplace_back();

                        if (!registry->find_artifact_by_id(artifactId, artifactEntry))
                        {
                            e.artifacts.pop_back();
                        }
                    }
                }
            }
        }

        return abDir;
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