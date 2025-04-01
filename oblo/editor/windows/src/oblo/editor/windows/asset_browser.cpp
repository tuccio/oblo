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
#include <oblo/editor/services/asset_editor_manager.hpp>
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

        struct rename_context
        {
            char buffer[255];

            bool isAppearing;

            const asset_browser_entry* activeRenameEntry;

            bool is_renaming() const;
            bool is_renaming(const asset_browser_entry* other) const;
            void start_renaming(const asset_browser_entry* other);
            void stop_renaming(bool apply);

            void init(string_view str)
            {
                constexpr auto maxLen = array_size(rename_context{}.buffer) - 1;
                const auto len = min<usize>(str.size(), maxLen);

                std::memcpy(buffer, str.data(), len);
                buffer[len] = '\0';

                isAppearing = true;
            }
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

        bool big_icon_widget(ImFont* bigIcons,
            ImU32 iconColor,
            const char* icon,
            ImU32 accentColor,
            const char* text,
            ImGuiID selectableId,
            bool* isSelected,
            rename_context* renameCtx = nullptr)
        {
            bool isPressed = false;

            const auto cursorPosition = ImGui::GetCursorScreenPos();

            const auto& style = ImGui::GetStyle();

            const f32 fontSize = bigIcons->FontSize;

            const auto selectableSize = ImVec2(ImGui::GetContentRegionAvail().x,
                fontSize + ImGui::GetTextLineHeightWithSpacing() + style.ItemInnerSpacing.y * 2);

            ImGui::PushID(selectableId);

            ImGui::BeginGroup();

            isPressed = ImGui::Selectable("##select",
                isSelected,
                ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick,
                selectableSize);

            const auto selectableItem = ImGui::GetCurrentContext()->LastItemData;

            ImGui::SetCursorScreenPos(cursorPosition);

            {
                const f32 itemPosX = cursorPosition.x;
                const f32 itemPosY = cursorPosition.y;

                ImGui::PushClipRect(ImVec2{itemPosX, itemPosY},
                    {itemPosX + selectableSize.x, itemPosY + selectableSize.y},
                    true);

                ImGui::PushFont(bigIcons);
                ImGui::PushStyleColor(ImGuiCol_Text, iconColor);

                const f32 iconTextWidth = ImGui::CalcTextSize(icon).x;

                const f32 iconTextPosition = cursorPosition.x + (selectableSize.x - iconTextWidth) * .5f;

                // Align the icon to the center of the button
                ImGui::SetCursorScreenPos({iconTextPosition, ImGui::GetCursorScreenPos().y});
                ImGui::TextUnformatted(icon);

                ImGui::PopStyleColor();
                ImGui::PopFont();

                if (!renameCtx)
                {
                    // Center align the label as well, unless the text is too big, then let it clip on the right
                    const f32 labelTextWidth = ImGui::CalcTextSize(text).x;
                    const f32 labelTextPosition = labelTextWidth > selectableSize.x
                        ? cursorPosition.x
                        : cursorPosition.x + (selectableSize.x - labelTextWidth) * .5f;

                    ImGui::SetCursorScreenPos({labelTextPosition, ImGui::GetCursorScreenPos().y});
                    ImGui::TextUnformatted(text);
                }
                else
                {
                    if (renameCtx->isAppearing)
                    {
                        ImGui::SetKeyboardFocusHere();
                        ImGui::SetScrollHereY();
                        renameCtx->isAppearing = false;
                    }

                    ImGui::SetNextItemWidth(selectableSize.x);

                    if (ImGui::InputText("##renameCtx",
                            renameCtx->buffer,
                            array_size(renameCtx->buffer),
                            ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        renameCtx->stop_renaming(true);
                    }
                    else if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                        ImGui::IsItemDeactivated())
                    {
                        renameCtx->stop_renaming(false);
                    }
                }

                ImDrawList* const drawList = ImGui::GetWindowDrawList();

                const f32 triangleSize = 5.f;

                const ImVec2 v1{itemPosX + selectableSize.x - triangleSize, itemPosY};
                const ImVec2 v2{itemPosX + selectableSize.x, itemPosY};
                const ImVec2 v3{itemPosX + selectableSize.x, itemPosY + triangleSize};

                drawList->AddTriangleFilled(v1, v2, v3, accentColor);

                ImGui::PopClipRect();
            }

            ImGui::SetCursorScreenPos(cursorPosition);

            ImGui::EndGroup();

            ImGui::SetLastItemData(selectableItem.ID,
                selectableItem.ItemFlags,
                selectableItem.StatusFlags,
                selectableItem.Rect);

            isPressed = isPressed && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            ImGui::PopID();

            return isPressed;
        }
    }

    struct asset_browser::impl
    {
        asset_registry* registry{};
        asset_editor_manager* assetEditors{};
        string_builder path;
        string_builder current;
        uuid expandedAsset{};
        deque<create_menu_item> createMenu;

        filesystem::directory_watcher assetDirWatcher;

        std::unordered_map<string_builder, asset_browser_directory, hash<string_builder>> assetBrowserEntries;
        deque<directory_tree_entry> directoryTree;

        ImFont* bigIconsFont{};

        u64 lastVersionId;

        const asset_browser_entry* selectedEntry{};
        const asset_browser_entry* activeRenameEntry{};

        string requestedDelete;

        rename_context renameCtx;

        void populate_asset_editors();
        void draw_popup_menu();

        void build_directory_tree();
        asset_browser_directory& get_or_build(const string_builder& p);

        void draw_modals();
        void draw_directory_tree_panel();
        void draw_main_panel(const window_update_context& ctx);

        void find_first_available(string_builder& builder, string_view extension);

        void delete_entry(const asset_browser_entry& entry);

        bool is_selected(const asset_browser_entry* other) const;
        void replace_selection(const asset_browser_entry* newSelection);

        void move_asset_to_directory(const uuid assetId, cstring_view directory);
    };

    asset_browser::asset_browser() = default;
    asset_browser::~asset_browser() = default;

    void asset_browser::init(const window_update_context& ctx)
    {
        m_impl = allocate_unique<impl>();

        m_impl->registry = ctx.services.find<asset_registry>();
        OBLO_ASSERT(m_impl->registry);

        m_impl->assetEditors = ctx.services.find<asset_editor_manager>();
        OBLO_ASSERT(m_impl->assetEditors);

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

            m_impl->draw_modals();

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
        renameCtx.stop_renaming(false);
        replace_selection(nullptr);
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
                auto name = p.filename().stem();

                auto& e = abDir.entries.emplace_back();
                e.kind = asset_browser_entry_kind::directory;
                e.path.append(p.native().c_str());
                e.name.append(name.native().c_str());
            }
            else if (p.extension() == AssetMetaExtension.c_str())
            {
                auto name = p.filename().stem();

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

    void asset_browser::impl::draw_modals()
    {
        if (!requestedDelete.empty())
        {
            constexpr const char* deletePopup = "Delete";

            bool isOpen = true;

            if (!ImGui::IsPopupOpen(deletePopup))
            {
                ImGui::OpenPopup(deletePopup);
            }

            if (ImGui::BeginPopupModal(deletePopup, &isOpen, ImGuiWindowFlags_AlwaysAutoResize))
            {
                string_builder builder;
                builder.format("Are you sure you want to delete '{}'?",
                    filesystem::filename(requestedDelete.as<cstring_view>()));

                ImGui::TextUnformatted(builder.c_str());
                ImGui::Dummy(ImVec2{0, 8});

                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                }

                ImGui::SetItemDefaultFocus();

                if (ImGui::Button("Yes", ImVec2(120, 0)))
                {
                    if (!filesystem::remove_all(requestedDelete).value_or(false))
                    {
                        log::error("Failed to delete {}", requestedDelete);
                    }

                    ImGui::CloseCurrentPopup();
                    requestedDelete.clear();
                }

                ImGui::SameLine();

                if (ImGui::Button("No", ImVec2(120, 0)))
                {
                    requestedDelete.clear();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
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
            b.clear().format(ICON_FA_FOLDER " {}##{}", dirName, e.path);

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

            if (ImGui::BeginDragDropTarget())
            {
                if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Asset))
                {
                    const uuid id = payloads::unpack_asset(assetPayload->Data);
                    move_asset_to_directory(id, e.path);
                }

                ImGui::EndDragDropTarget();
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

    namespace
    {
        struct dynamic_table_layout
        {
            u32 columns;

            u32 currentColumnIndex;

            void init(u32 cols)
            {
                columns = cols;
                currentColumnIndex = cols - 1;
            }

            void next_element()
            {
                if (++currentColumnIndex == columns)
                {
                    currentColumnIndex = 0;

                    ImGui::TableNextRow();
                }

                ImGui::TableNextColumn();
            }
        };
    }

    void asset_browser::impl::draw_main_panel(const window_update_context& ctx)
    {
        if (!ImGui::BeginChild("#main_panel"))
        {
            ImGui::EndChild();
            return;
        }

        const bool hasFocus = ImGui::IsWindowFocused();

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

            dynamic_table_layout layout;
            layout.init(columns);

            if (!dir.isRoot)
            {
                layout.next_element();

                if (bool isSelected = false; big_icon_widget(bigIconsFont,
                        g_DirectoryColor,
                        ICON_FA_ARROW_LEFT,
                        g_Transparent,
                        "Back",
                        ImGui::GetID("##back"),
                        &isSelected))
                {
                    current.append_path("..").make_canonical_path();
                }

                if (ImGui::BeginItemTooltip())
                {
                    ImGui::TextUnformatted("Back to parent folder");
                    ImGui::EndTooltip();
                }
            }

            for (const auto& entry : dir.entries)
            {
                layout.next_element();

                switch (entry.kind)
                {
                case asset_browser_entry_kind::directory: {
                    builder.clear().format("##{}", entry.path);

                    ImGui::PushID(builder.c_str());

                    bool isSelected = is_selected(&entry);

                    auto* const entryRename = renameCtx.is_renaming(&entry) ? &renameCtx : nullptr;

                    if (big_icon_widget(bigIconsFont,
                            g_DirectoryColor,
                            ICON_FA_FOLDER,
                            g_Transparent,
                            entry.name.c_str(),
                            ImGui::GetID("##dir"),
                            &isSelected,
                            entryRename))
                    {
                        current.clear().append(entry.path).make_canonical_path();
                    }

                    if (isSelected)
                    {
                        replace_selection(&entry);
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Asset))
                        {
                            const uuid id = payloads::unpack_asset(assetPayload->Data);
                            move_asset_to_directory(id, entry.path);
                        }

                        ImGui::EndDragDropTarget();
                    }

                    if (ImGui::BeginPopupContextItem("##dirctx"))
                    {
                        // The popup actually refers to the asset we release right click on, make it clear through
                        // selection
                        replace_selection(&entry);

                        if (ImGui::MenuItem("Rename"))
                        {
                            renameCtx.start_renaming(&entry);
                        }

                        if (ImGui::MenuItem("Delete"))
                        {
                            delete_entry(*selectedEntry);
                        }

                        if (ImGui::MenuItem("Open in Explorer"))
                        {
                            platform::open_folder(entry.path.view());
                        }

                        ImGui::EndPopup();
                    }

                    if (ImGui::BeginItemTooltip())
                    {
                        ImGui::TextUnformatted(entry.name.c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::PopID();
                }

                break;
                case asset_browser_entry_kind::asset: {
                    const asset_meta& meta = entry.meta;

                    ImGui::PushID(int(hash_all<std::hash>(meta.assetId)));

                    builder.clear().format("##{}", entry.path);

                    const auto assetColorId =
                        hash_all<hash>(meta.nativeAssetType, meta.typeHint) % array_size(g_Colors);
                    const auto assetColor = g_Colors[assetColorId];

                    bool isSelected = is_selected(&entry);

                    auto* const entryRename = renameCtx.is_renaming(&entry) ? &renameCtx : nullptr;

                    const bool isPressed = big_icon_widget(bigIconsFont,
                        g_FileColor,
                        ICON_FA_FILE,
                        assetColor,
                        entry.name.c_str(),
                        ImGui::GetID(builder.c_str()),
                        &isSelected,
                        entryRename);

                    if (ImGui::BeginItemTooltip())
                    {
                        ImGui::TextUnformatted(entry.name.c_str());

                        string_builder uuidStr;
                        uuidStr.format("{}", meta.assetId);

                        ImGui::TextDisabled("%s", uuidStr.c_str());

                        ImGui::EndTooltip();
                    }

                    if (isSelected)
                    {
                        replace_selection(&entry);
                    }

                    if (isPressed)
                    {
                        bool shouldExpandAsset = true;

                        if (!meta.nativeAssetType.is_nil())
                        {
                            const auto r =
                                assetEditors->open_editor(ctx.windowManager, meta.assetId, meta.nativeAssetType);

                            shouldExpandAsset =
                                !r.has_value() && r.error() == asset_editor_manager::open_error::no_such_type;
                        }

                        if (shouldExpandAsset)
                        {
                            expandedAsset = expandedAsset == meta.assetId ? uuid{} : meta.assetId;
                        }
                    }
                    else if (!meta.mainArtifactHint.is_nil() && ImGui::BeginDragDropSource())
                    {
                        const auto payload = payloads::pack_artifact(meta.assetId);
                        ImGui::SetDragDropPayload(payloads::Asset, &payload, sizeof(drag_and_drop_payload));
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginPopupContextItem("##assetctx"))
                    {
                        // The popup actually refers to the asset we release right click on, make it clear through
                        // selection
                        replace_selection(&entry);

                        if (ImGui::MenuItem("Rename"))
                        {
                            renameCtx.start_renaming(&entry);
                        }

                        if (ImGui::MenuItem("Delete"))
                        {
                            delete_entry(entry);
                        }

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
                            layout.next_element();

                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                ImGui::GetColorU32(ImGuiCol_FrameBgActive));

                            ImGui::PushID(int(hash_all<std::hash>(artifactMeta.artifactId)));

                            const auto artifactName =
                                artifactMeta.name.empty() ? "Unnamed Artifact" : artifactMeta.name.c_str();

                            const auto artifactColorId = hash_all<hash>(artifactMeta.type) % array_size(g_Colors);
                            const auto artifactColor = g_Colors[artifactColorId];

                            big_icon_widget(bigIconsFont,
                                g_FileColor,
                                ICON_FA_FILE_LINES,
                                artifactColor,
                                artifactName,
                                ImGui::GetID(builder.c_str()),
                                &isSelected);

                            if (!artifactMeta.artifactId.is_nil() && ImGui::BeginDragDropSource())
                            {
                                const auto payload = payloads::pack_artifact(artifactMeta.artifactId);
                                ImGui::SetDragDropPayload(payloads::Artifact, &payload, sizeof(drag_and_drop_payload));
                                ImGui::EndDragDropSource();
                            }

                            if (ImGui::BeginItemTooltip())
                            {
                                ImGui::TextUnformatted(artifactName);

                                string_builder uuidStr;
                                uuidStr.format("{}", artifactMeta.artifactId);

                                ImGui::TextDisabled("%s", uuidStr.c_str());

                                ImGui::EndTooltip();
                            }

                            if (ImGui::BeginPopupContextItem("##ctx"))
                            {
                                if (ImGui::MenuItem("Open Artifact in Explorer"))
                                {
                                    string_builder artifactPath;
                                    if (registry->get_artifact_path(artifactMeta.artifactId, artifactPath))
                                    {
                                        artifactPath.parent_path();
                                        platform::open_folder(artifactPath.view());
                                    }
                                }

                                ImGui::EndPopup();
                            }

                            ImGui::PopID();
                        }
                    }
                }
                break;
                }
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleColor(styleVars);

        if (hasFocus)
        {
            if (selectedEntry)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Delete))
                {
                    delete_entry(*selectedEntry);
                }

                if (ImGui::IsKeyPressed(ImGuiKey_F2))
                {
                    renameCtx.start_renaming(selectedEntry);
                }
            }
        }

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

    void asset_browser::impl::delete_entry(const asset_browser_entry& entry)
    {
        requestedDelete = entry.path.as<string_view>();
    }

    bool asset_browser::impl::is_selected(const asset_browser_entry* other) const
    {
        return selectedEntry == other;
    }

    void asset_browser::impl::replace_selection(const asset_browser_entry* newSelection)
    {
        if (selectedEntry != newSelection)
        {
            selectedEntry = newSelection;
        }
    }

    void asset_browser::impl::move_asset_to_directory(const uuid assetId, cstring_view directory)
    {
        asset_meta assetMeta;

        if (registry->find_asset_by_id(assetId, assetMeta))
        {
            string_builder oldPath;

            string_builder newPath;
            newPath.append(directory).append_path_separator();

            if (registry->get_asset_name(assetId, newPath) && registry->get_asset_directory(assetId, oldPath) &&
                registry->get_asset_name(assetId, oldPath.append_path_separator()))
            {
                oldPath.append(AssetMetaExtension);
                newPath.append(AssetMetaExtension);

                filesystem::rename(oldPath, newPath).assert_value();
            }
        }
    }

    bool rename_context::is_renaming() const
    {
        return activeRenameEntry != nullptr;
    }

    bool rename_context::is_renaming(const asset_browser_entry* other) const
    {
        return activeRenameEntry == other;
    }

    void rename_context::start_renaming(const asset_browser_entry* other)
    {
        if (is_renaming())
        {
            stop_renaming(false);
        }

        activeRenameEntry = other;

        if (activeRenameEntry)
        {
            init(activeRenameEntry->name.view());
        }
    }

    void rename_context::stop_renaming(bool apply)
    {
        if (!activeRenameEntry)
        {
            return;
        }

        if (apply)
        {
            string_builder newName = activeRenameEntry->path;
            newName.parent_path().append_path(buffer);

            if (activeRenameEntry->kind == asset_browser_entry_kind::asset)
            {
                newName.append(AssetMetaExtension);
            }

            if (!filesystem::rename(activeRenameEntry->path, newName).value_or(false))
            {
                log::debug("Failed to rename {} to {}", activeRenameEntry->path, newName);
            }
        }

        activeRenameEntry = nullptr;
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
                    const auto filename = filesystem::stem(filesystem::filename(file.view()));

                    string_builder available = current;
                    available.append_path(filename);
                    find_first_available(available, AssetMetaExtension);

                    const auto r = registry->import(file.view(),
                        current.view(),
                        filesystem::stem(filesystem::filename(available.view())),
                        data_document{});

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
                                current.view(),
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