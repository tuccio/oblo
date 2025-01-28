#include <oblo/editor/windows/asset_browser.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/filesystem/directory_watcher.hpp>
#include <oblo/core/filesystem/filesystem.hpp>
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
#include <imgui_internal.h>

#include <IconsFontAwesome6.h>

#include <filesystem>
#include <unordered_map>

namespace oblo::editor
{
    namespace
    {
        enum class asset_browser_entry_kind : u8
        {
            directory,
            asset,
        };

        struct asset_browser_entry
        {
            asset_browser_entry_kind kind;
            asset_meta meta;
            deque<artifact_meta> artifacts;
            string_builder path;
            string_builder name;
        };

        struct directory_tree_entry
        {
            string_builder path;
            u32 firstChild{};
            u32 lastChild{};
        };

        struct asset_browser_directory
        {
            deque<asset_browser_entry> entries;
            bool isRoot;
        };

        struct create_menu_item
        {
            cstring_view name;
            asset_create_fn create{};
        };

        constexpr u32 g_Transparent = 0x00000000;
        constexpr u32 g_DirectoryColor = 0xFF7CC9E6;
        constexpr u32 g_FileColor = 0xFFDCEEEE;

        constexpr u32 g_Colors[] = {
            0xFF82DC59,
            0xFFF87574,
            0xFF58B5E1,
            0xFF2CC0A1,
            0xFFF4BB8F,
            0xFFEBC30E,
            0xFFBF83F8,
            0xFFF79302,
        };

        bool big_icon_button(ImFont* bigIcons,
            ImU32 iconColor,
            const char* icon,
            ImU32 accentColor,
            const char* text,
            ImGuiID selectableId,
            bool* isSelected)
        {
            bool pressed = false;

            const auto cursorPosition = ImGui::GetCursorPos();

            const f32 fontSize = bigIcons->FontSize;
            // const auto selectableSize = ImGui::GetContentRegionAvail();
            const auto selectableSize =
                ImVec2(ImGui::GetContentRegionAvail().x, fontSize + ImGui::GetTextLineHeightWithSpacing());

            ImGui::PushID(selectableId);

            ImGui::BeginGroup();

            pressed = ImGui::Selectable("##select",
                isSelected,
                ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick,
                selectableSize);

            const auto selectableItem = ImGui::GetCurrentContext()->LastItemData;

            ImGui::SetCursorPos(cursorPosition);

            {
                const auto windowPos = ImGui::GetWindowPos();
                const f32 itemPosX = windowPos.x + cursorPosition.x;
                const f32 itemPosY = windowPos.y + cursorPosition.y;

                ImGui::PushClipRect(ImVec2{itemPosX, itemPosY},
                    {itemPosX + selectableSize.x, itemPosY + selectableSize.y},
                    true);

                // const auto itemRectMin = ImGui::GetItemRectMin();
                // const auto itemRectMax = ImGui::GetItemRectMax();
                // ImGui::PushClipRect(itemRectMin, itemRectMax, true);

                ImGui::PushFont(bigIcons);
                ImGui::PushStyleColor(ImGuiCol_Text, iconColor);

                const f32 textWidth = ImGui::CalcTextSize(icon).x;

                const f32 textPosition = cursorPosition.x + (selectableSize.x - textWidth) * .5f;

                // Align the icon to the center of the button
                ImGui::SetCursorPosX(textPosition);
                ImGui::TextUnformatted(icon);

                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::TextUnformatted(text);

                ImDrawList* const drawList = ImGui::GetWindowDrawList();

                const f32 triangleSize = 5.f;

                const ImVec2 v1{itemPosX + selectableSize.x - triangleSize, itemPosY};
                const ImVec2 v2{itemPosX + selectableSize.x, itemPosY};
                const ImVec2 v3{itemPosX + selectableSize.x, itemPosY + triangleSize};

                drawList->AddTriangleFilled(v1, v2, v3, accentColor);

                ImGui::PopClipRect();
            }

            ImGui::SetCursorPos(cursorPosition);

            ImGui::EndGroup();

            ImGui::SetLastItemData(selectableItem.ID,
                selectableItem.InFlags,
                selectableItem.StatusFlags,
                selectableItem.Rect);

            ImGui::PopID();

            pressed = pressed && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            return pressed;
        }
    }

    struct asset_browser::impl
    {
        asset_registry* registry{};
        string_builder path;
        string_builder current;
        uuid expandedAsset{};
        deque<create_menu_item> createMenu;

        filesystem::directory_watcher assetDirWatcher;

        std::unordered_map<uuid, asset_editor_create_fn> editorsLookup;

        std::unordered_map<string_builder, asset_browser_directory, hash<string_builder>> assetBrowserEntries;
        deque<directory_tree_entry> directoryTree;

        ImFont* bigIconsFont{};

        u64 lastVersionId;

        void populate_asset_editors();
        void draw_popup_menu();

        void build_directory_tree();
        asset_browser_directory& get_or_build(const string_builder& p);

        void draw_directory_tree_panel();
        void draw_main_panel(const window_update_context& ctx);

        void find_first_available(string_builder& builder, string_view extension);
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

        auto& fonts = ImGui::GetIO().Fonts;

        m_impl->bigIconsFont = fonts->Fonts.back();
    }

    bool asset_browser::update(const window_update_context& ctx)
    {
        bool open{true};

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        i32 varsToPop = 1;

        if (ImGui::Begin("Asset Browser", &open))
        {
            ImGui::PopStyleVar(varsToPop);

            --varsToPop;

            bool requiresRefresh{};

            if (!m_impl->assetDirWatcher.process([&](auto&&) { requiresRefresh = true; }))
            {
                log::debug("Asset browser watch processing on '{}' failed", m_impl->path);
            }

            if (m_impl->directoryTree.empty() || requiresRefresh)
            {
                m_impl->build_directory_tree();
            }

            if (const auto vId = m_impl->registry->get_version_id(); requiresRefresh || m_impl->lastVersionId != vId)
            {
                m_impl->assetBrowserEntries.clear();
                m_impl->lastVersionId = vId;
            }

            m_impl->draw_directory_tree_panel();

            ImGui::SameLine();

            m_impl->draw_main_panel(ctx);
        }

        ImGui::End();

        ImGui::PopStyleVar(varsToPop);

        return open;
    }

    void asset_browser::impl::build_directory_tree()
    {
        directoryTree.clear();

        std::error_code ec;

        struct queue_item
        {
            std::filesystem::path path;
            i64 parent{-1};
        };

        deque<queue_item> queue;
        queue.emplace_back(path.as<std::string>(), -1);

        while (!queue.empty())
        {
            auto& dir = queue.front();

            const u32 entryIdx = directoryTree.size32();

            auto& entry = directoryTree.emplace_back();
            entry.path.append(dir.path.native().c_str());

            entry.firstChild = directoryTree.size32() + queue.size32() - 1;

            for (auto&& fsEntry : std::filesystem::directory_iterator{dir.path, ec})
            {
                const auto& p = fsEntry.path();

                if (std::filesystem::is_directory(p))
                {
                    queue.emplace_back(p, entryIdx);
                }
            }

            entry.lastChild = directoryTree.size32() + queue.size32() - 1;

            queue.pop_front();
        }
    }

    asset_browser_directory& asset_browser::impl::get_or_build(const string_builder& assetDir)
    {
        std::error_code ec;

        // TODO: if it's not in the asset directory, maybe reutrn the root

        const auto [it, isNew] = assetBrowserEntries.emplace(assetDir, asset_browser_directory{});
        auto& abDir = it->second;

        if (!isNew)
        {
            return abDir;
        }

        if (assetDir == path)
        {
            abDir.isRoot = true;
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

    void asset_browser::impl::draw_directory_tree_panel()
    {
        if (!ImGui::BeginChild("#tree_panel", ImVec2{150, 0}, true))
        {
            ImGui::EndChild();
            return;
        }

        string_builder b;

        struct draw_node_info
        {
            usize index;
            u32 ancestorsToPop;
        };

        deque<draw_node_info> stack;

        if (!directoryTree.empty())
        {
            stack.push_back_default() = {
                .index = 0,
                .ancestorsToPop = 0,
            };
        }

        while (!stack.empty())
        {
            const draw_node_info info = stack.back();
            stack.pop_back();

            const auto& e = directoryTree[info.index];

            const auto dirName = filesystem::filename(e.path.view());
            b.clear().format("{}##{}", dirName, e.path);

            i32 nodeFlags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                (info.index == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);

            const bool isLeaf = e.firstChild == e.lastChild;

            if (isLeaf)
            {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            }

            if (e.path == current)
            {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }

            const bool expanded = ImGui::TreeNodeEx(b.c_str(), nodeFlags);

            if (ImGui::IsItemActivated())
            {
                current = e.path;
            }

            u32 nodesToPop = info.ancestorsToPop;

            if (expanded)
            {
                ++nodesToPop;

                const auto firstChildIdx = stack.size();

                const u32 childrenCount = e.lastChild - e.firstChild;

                for (u32 i = 1; i <= childrenCount; ++i)
                {
                    const u32 reverseIndex = e.lastChild - i;
                    stack.emplace_back(reverseIndex);
                }

                // The first will be processed last
                if (firstChildIdx != stack.size())
                {
                    stack[firstChildIdx].ancestorsToPop = nodesToPop;
                }
            }

            if (isLeaf || !expanded)
            {
                for (u32 i = 0; i < nodesToPop; ++i)
                {
                    ImGui::TreePop();
                }
            }
        }

        ImGui::EndChild();
    }

    namespace
    {
        i32 setup_table_style()
        {
            const ImVec4 windowColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

            ImGui::PushStyleColor(ImGuiCol_TableRowBg, windowColor);
            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, windowColor);
            ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            return 4;
        }
    }

    void asset_browser::impl::draw_main_panel(const window_update_context& ctx)
    {
        if (!ImGui::BeginChild("#main_panel"))
        {
            ImGui::EndChild();
            return;
        }

        draw_popup_menu();

        string_builder builder;

        const asset_browser_directory& dir = get_or_build(current);

        const f32 entryWidth = bigIconsFont->FontSize + 4.f * ImGui::GetStyle().ItemSpacing.x;

        u32 columns = max(u32(ImGui::GetContentRegionAvail().x / entryWidth), 1u);

        const i32 styleVars = setup_table_style();

        if (ImGui::BeginTable("#grid", columns, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
        {
            for (u32 i = 0; i < columns; ++i)
            {
                builder.format("##col{}", i);
                ImGui::TableSetupColumn(builder.c_str());
            }

            usize entryIndex{};

            u32 currentColumnIndex = 0;

            if (!dir.isRoot)
            {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                if (bool isSelected = false; big_icon_button(bigIconsFont,
                        g_DirectoryColor,
                        ICON_FA_CIRCLE_CHEVRON_UP,
                        g_Transparent,
                        "Back",
                        ImGui::GetID("##back"),
                        &isSelected))
                {
                    current.append_path("..").make_canonical_path();
                }

                currentColumnIndex = 1;
            }

            while (entryIndex < dir.entries.size())
            {
                if (currentColumnIndex == 0)
                {
                    ImGui::TableNextRow();
                }

                for (; currentColumnIndex < columns; ++currentColumnIndex)
                {
                    ImGui::TableNextColumn();

                    if (entryIndex >= dir.entries.size())
                    {
                        break;
                    }

                    auto& entry = dir.entries[entryIndex];

                    switch (entry.kind)
                    {
                    case asset_browser_entry_kind::directory:
                        builder.clear().format("##{}", entry.path);

                        if (bool isSelected = false; big_icon_button(bigIconsFont,
                                g_DirectoryColor,
                                ICON_FA_FOLDER,
                                g_Transparent,
                                entry.name.c_str(),
                                ImGui::GetID(builder.c_str()),
                                &isSelected))
                        {
                            current.clear().append(entry.path).make_canonical_path();
                        }

                        break;
                    case asset_browser_entry_kind::asset: {
                        const asset_meta& meta = entry.meta;

                        ImGui::PushID(int(hash_all<std::hash>(meta.assetId)));

                        builder.clear().format("##{}", entry.path);

                        const auto typeHash = hash_all<hash>(meta.nativeAssetType, meta.typeHint);
                        const auto colorId = typeHash % array_size(g_Colors);
                        const auto accentColor = g_Colors[colorId];

                        if (bool isSelected = false; big_icon_button(bigIconsFont,
                                g_FileColor,
                                ICON_FA_FILE,
                                accentColor,
                                entry.name.c_str(),
                                ImGui::GetID(builder.c_str()),
                                &isSelected))
                        {
                            if (const auto it = editorsLookup.find(meta.nativeAssetType); it != editorsLookup.end())
                            {
                                const asset_editor_create_fn createWindow = it->second;
                                createWindow(ctx.windowManager,
                                    ctx.windowManager.get_parent(ctx.windowHandle),
                                    meta.assetId);
                            }
                            else
                            {
                                // expandedAsset = expandedAsset == meta.assetId ? uuid{} : meta.assetId;
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
                                if (!registry->process(meta.assetId))
                                {
                                    log::error("Failed to reimport {}", meta.assetId);
                                }
                            }

                            if (ImGui::MenuItem("Open Source in Explorer"))
                            {
                                string_builder sourcePath;
                                if (registry->get_source_directory(meta.assetId, sourcePath))
                                {
                                    platform::open_folder(sourcePath.view());
                                }
                            }

                            ImGui::EndPopup();
                        }

                        ImGui::PopID();

                        if (expandedAsset == meta.assetId)
                        {
                            for (const auto& artifactMeta : entry.artifacts)
                            {
                                ImGui::PushID(int(hash_all<std::hash>(artifactMeta.artifactId)));

                                ImGui::Button(
                                    artifactMeta.name.empty() ? "Unnamed Artifact" : artifactMeta.name.c_str());

                                ImGui::PopID();

                                if (ImGui::BeginDragDropSource())
                                {
                                    const auto payload = payloads::pack_uuid(artifactMeta.artifactId);

                                    ImGui::SetDragDropPayload(payloads::Resource,
                                        &payload,
                                        sizeof(drag_and_drop_payload));

                                    ImGui::EndDragDropSource();
                                }
                            }
                        }
                    }
                    break;
                    }

                    ++entryIndex;
                }

                currentColumnIndex = currentColumnIndex % columns;
            }

            // Finish the last row
            // for (; currentColumnIndex < columns; ++currentColumnIndex)
            //{
            //    ImGui::Dummy({});
            //    ImGui::TableNextColumn();
            //}

            //    ImGui::TableNextRow();

            ImGui::EndTable();
        }

        ImGui::PopStyleColor(styleVars);

        ImGui::EndChild();
    }

    void asset_browser::impl::find_first_available(string_builder& builder, string_view extension)
    {
        constexpr u32 maxTries = 100;

        const auto currentSize = builder.size();

        u32 count = 0;

        do
        {
            builder.append(extension);

            if (!filesystem::exists(builder).value_or(true))
            {
                break;
            }

            builder.resize(currentSize);
            builder.format(" {}", ++count);

        } while (count < maxTries);
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

            if (!createMenu.empty() && ImGui::BeginMenu("New"))
            {
                if (ImGui::MenuItem("Folder"))
                {
                    string_builder directory = current;
                    directory.append_path("New Folder");

                    find_first_available(directory, "");

                    if (!filesystem::create_directories(directory))
                    {
                        log::error("Failed to create new directory {}", directory);
                    }
                }

                if (ImGui::BeginMenu("Asset"))
                {
                    for (const auto& item : createMenu)
                    {
                        if (ImGui::MenuItem(item.name.c_str()))
                        {
                            string_builder assetPath = current;
                            assetPath.append_path("New ").append(item.name);

                            find_first_available(assetPath, AssetMetaExtension);

                            const auto r = registry->create_asset(item.create(),
                                current,
                                filesystem::stem(filesystem::filename(assetPath.view())));

                            if (!r)
                            {
                                log::error("Failed to create new asset {}", item.name);
                            }
                        }
                    }

                    ImGui::EndMenu();
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