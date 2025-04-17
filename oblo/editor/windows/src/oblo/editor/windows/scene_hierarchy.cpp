#include <oblo/editor/windows/scene_hierarchy.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/services/editor_world.hpp>
#include <oblo/editor/services/selected_entities.hpp>
#include <oblo/editor/utility/entity_utility.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/math/quaternion.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/scene/components/children_component.hpp>
#include <oblo/scene/components/parent_component.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include <imgui.h>

#include <imgui_internal.h>

namespace oblo::editor
{
    void scene_hierarchy::init(const window_update_context& ctx)
    {
        m_editorWorld = ctx.services.find<editor_world>();
    }

    bool scene_hierarchy::update(const window_update_context&)
    {
        bool open{true};

        final_act_queue defer;

        if (ImGui::Begin("Hierarchy", &open))
        {
            auto* const entityRegistry = m_editorWorld->get_entity_registry();
            auto* const selection = m_editorWorld->get_selected_entities();

            auto* const window = ImGui::GetCurrentWindow();

            if (ImGui::BeginDragDropTargetCustom(window->ContentRegionRect, window->ID))
            {
                if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Entity))
                {
                    const ecs::entity newChild = payloads::unpack_entity(assetPayload->Data);
                    ecs_utility::reparent_entity(*entityRegistry, newChild, {});
                }

                ImGui::EndDragDropTarget();
            }

            const auto hasFocus = ImGui::IsWindowFocused();

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
            {
                ImGui::OpenPopup("##context");
            }

            if (ImGui::BeginPopupContextItem("##context", ImGuiPopupFlags_None))
            {
                if (ImGui::MenuItem("Create Entity"))
                {
                    const auto e = ecs_utility::create_named_physical_entity(*entityRegistry,
                        "New Entity",
                        {},
                        {},
                        quaternion::identity(),
                        vec3::splat(1));

                    selection->clear();
                    selection->add(e);
                    selection->push_refresh_event();
                }

                const std::span selectedEntities = selection->get();

                if (!selectedEntities.empty())
                {
                    if (ImGui::MenuItem(selectedEntities.size() == 1 ? "Delete Entity" : "Delete Entities"))
                    {
                        for (const auto e : selectedEntities)
                        {
                            ecs_utility::destroy_hierarchy(*entityRegistry, e);
                        }

                        selection->clear();
                    }
                }

                ImGui::EndPopup();
            }
            else
            {
                if (hasFocus && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    for (const auto e : selection->get())
                    {
                        ecs_utility::destroy_hierarchy(*entityRegistry, e);
                    }

                    selection->clear();
                }
            }

            ecs::entity entityToSelect{};

            if (const auto eventId = selection->get_last_refresh_event_id(); m_lastRefreshEvent != eventId)
            {
                const auto selectedEntities = selection->get();

                if (selectedEntities.size() == 1)
                {
                    entityToSelect = selectedEntities.front();
                }

                m_lastRefreshEvent = eventId;
            }

            const auto rootsRange = entityRegistry->range<>().exclude<parent_component>();

            struct entity_stack_entry
            {
                ecs::entity id;
                u32 ancestorsToPop;
                bool forceExpand;
            };

            deque<entity_stack_entry> stack;

            for (const auto& chunk : rootsRange)
            {
                for (ecs::entity root : chunk.get<ecs::entity>())
                {
                    bool isSelectedEntityInTree = false;

                    if (entityToSelect != ecs::entity{} &&
                        ecs_utility::find_root(*entityRegistry, entityToSelect) == root)
                    {
                        isSelectedEntityInTree = true;
                    }

                    stack.assign(1,
                        {
                            .id = root,
                            .ancestorsToPop = 0,
                            .forceExpand = isSelectedEntityInTree,
                        });

                    while (!stack.empty())
                    {
                        const auto info = stack.back();
                        stack.pop_back();

                        const ecs::entity e = info.id;
                        const bool forceExpand = info.forceExpand;

                        const children_component* cc = entityRegistry->try_get<children_component>(e);

                        ImGuiTreeNodeFlags flags{ImGuiTreeNodeFlags_OpenOnArrow};

                        const bool isLeaf = !cc || cc->children.empty();

                        if (isLeaf)
                        {
                            flags |= ImGuiTreeNodeFlags_Leaf;
                        }

                        if (selection->contains(e))
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;

                            if (e == entityToSelect)
                            {
                                ImGui::SetScrollHereY();
                            }
                        }

                        auto* const name = entity_utility::get_name_cstr(*entityRegistry, e);

                        if (forceExpand)
                        {
                            ImGui::SetNextItemOpen(true);
                        }

                        const bool expanded =
                            ImGui::TreeNodeEx(reinterpret_cast<void*>(intptr(e.value)), flags, "%s", name);

                        if (ImGui::BeginDragDropSource())
                        {
                            ImGui::TextUnformatted(name);

                            const auto payload = payloads::pack_entity(e);
                            ImGui::SetDragDropPayload(payloads::Entity, &payload, sizeof(drag_and_drop_payload));
                            ImGui::EndDragDropSource();
                        }

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Entity))
                            {
                                // Make sure the node is expanded after reparenting
                                ImGui::ActivateItemByID(ImGui::GetItemID());

                                const ecs::entity newChild = payloads::unpack_entity(assetPayload->Data);

                                defer.push([entityRegistry, newChild, e]
                                    { ecs_utility::reparent_entity(*entityRegistry, newChild, e); });
                            }

                            ImGui::EndDragDropTarget();
                        }

                        if (ImGui::IsItemClicked())
                        {
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                selection->clear();
                            }

                            selection->add(e);
                        }

                        u32 nodesToPop = info.ancestorsToPop;

                        if (expanded)
                        {
                            ++nodesToPop;

                            if (!isLeaf)
                            {
                                const auto firstChildIdx = stack.size();

                                for (const auto child : reverse_range(cc->children))
                                {
                                    stack.push_back(entity_stack_entry{
                                        .id = child,
                                        .ancestorsToPop = 0,
                                        // We can stop expanding once we found our entity
                                        .forceExpand = forceExpand && entityToSelect != e,
                                    });
                                }

                                // The first will be processed last, that one will pop all ancestors
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
                }
            }

            ImGui::End();
        }

        return open;
    }
}