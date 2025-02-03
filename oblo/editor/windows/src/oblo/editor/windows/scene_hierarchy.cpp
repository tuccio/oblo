#include <oblo/editor/windows/scene_hierarchy.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/iterator/reverse_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/editor/data/drag_and_drop_payload.hpp>
#include <oblo/editor/service_context.hpp>
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
        m_registry = ctx.services.find<ecs::entity_registry>();
        m_selection = ctx.services.find<selected_entities>();
    }

    bool scene_hierarchy::update(const window_update_context&)
    {
        bool open{true};

        final_act_queue defer;

        if (ImGui::Begin("Hierarchy", &open))
        {
            auto* const window = ImGui::GetCurrentWindow();

            if (ImGui::BeginDragDropTargetCustom(window->ContentRegionRect, window->ID))
            {
                if (auto* const assetPayload = ImGui::AcceptDragDropPayload(payloads::Entity))
                {
                    const ecs::entity newChild = payloads::unpack_entity(assetPayload->Data);
                    ecs_utility::reparent_entity(*m_registry, newChild, {});
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
                    const auto e = ecs_utility::create_named_physical_entity(*m_registry,
                        "New Entity",
                        {},
                        {},
                        quaternion::identity(),
                        vec3::splat(1));

                    m_selection->clear();
                    m_selection->add(e);
                    m_selection->push_refresh_event();
                }

                const std::span selectedEntities = m_selection->get();

                if (!selectedEntities.empty())
                {
                    if (ImGui::MenuItem(selectedEntities.size() == 1 ? "Delete Entity" : "Delete Entities"))
                    {
                        for (const auto e : selectedEntities)
                        {
                            m_registry->destroy(e);
                        }

                        m_selection->clear();
                    }
                }

                ImGui::EndPopup();
            }
            else
            {
                if (hasFocus && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                {
                    for (const auto e : m_selection->get())
                    {
                        m_registry->destroy(e);
                    }

                    m_selection->clear();
                }
            }

            ecs::entity entityToSelect{};

            if (const auto eventId = m_selection->get_last_refresh_event_id(); m_lastRefreshEvent != eventId)
            {
                const auto selection = m_selection->get();

                if (selection.size() == 1)
                {
                    entityToSelect = selection.front();
                }

                m_lastRefreshEvent = eventId;
            }

            const auto rootsRange = m_registry->range<>().exclude<parent_component>();

            struct entity_stack_entry
            {
                ecs::entity id;
                u32 ancestorsToPop;
            };

            deque<entity_stack_entry> stack;

            for (const auto& chunk : rootsRange)
            {
                for (ecs::entity root : chunk.get<ecs::entity>())
                {
                    stack.assign(1,
                        {
                            .id = root,
                            .ancestorsToPop = 0,
                        });

                    while (!stack.empty())
                    {
                        const auto info = stack.back();
                        stack.pop_back();

                        const ecs::entity e = info.id;

                        const children_component* cc = m_registry->try_get<children_component>(e);

                        ImGuiTreeNodeFlags flags{};

                        const bool isLeaf = !cc || cc->children.empty();

                        u32 childrenCount = 0;

                        if (isLeaf)
                        {
                            flags |= ImGuiTreeNodeFlags_Leaf;
                        }
                        else
                        {
                            childrenCount = cc->children.size32();
                        }

                        if (m_selection->contains(e))
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;

                            if (e == entityToSelect)
                            {
                                ImGui::SetScrollHereY();
                            }
                        }

                        auto* const name = entity_utility::get_name_cstr(*m_registry, e);

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

                                defer.push(
                                    [this, newChild, e] { ecs_utility::reparent_entity(*m_registry, newChild, e); });
                            }

                            ImGui::EndDragDropTarget();
                        }

                        if (ImGui::IsItemClicked())
                        {
                            if (!ImGui::GetIO().KeyCtrl)
                            {
                                m_selection->clear();
                            }

                            m_selection->add(e);
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
                                    stack.push_back(entity_stack_entry{.id = child, .ancestorsToPop = 0});
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