#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <oblo/core/uuid.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/nodes/editor/node_editor.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_graph_registry.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>

namespace oblo
{
    namespace
    {
        constexpr f32 g_MinZoom{.2f};
        constexpr f32 g_MaxZoom{1.5f};
        constexpr f32 g_ZoomSpeed{.1f};

        constexpr f32 g_TitleBarHeight{24.f};
        constexpr ImVec2 g_TitleTextMargin{8.f, 6.f};
        constexpr f32 g_NodeRounding{6.f};

        constexpr u32 g_TitleBackground{IM_COL32(80, 130, 200, 255)};
    }

    struct node_editor::impl
    {
        void update();

        ImVec2 world_to_screen(const ImVec2& world, const ImVec2& origin) const noexcept
        {
            return origin + world * zoom;
        }

        ImVec2 screen_to_world(const ImVec2& world, const ImVec2& origin) const noexcept
        {
            return (world - origin) / zoom;
        }

        node_graph* graph{};
        ImVec2 panOffset{0.f, 0.f};
        ImVec2 dragOffset{0.f, 0.f};
        f32 zoom{1.f};
        h32<node_graph_node> draggedNode{};
        h32<node_graph_node> selectedNode{};
    };

    node_editor::node_editor() = default;

    node_editor::node_editor(node_editor&&) noexcept = default;

    node_editor::~node_editor() = default;

    node_editor& node_editor::operator=(node_editor&&) noexcept = default;

    void node_editor::init(node_graph& g)
    {
        m_impl = allocate_unique<impl>();
        m_impl->graph = &g;

        // Add two constant nodes and an add node for testing purposes
        m_impl->graph->add_node("53b6e2bf-f0fc-43e3-ade4-25f3a74a42e1"_uuid);
        m_impl->graph->add_node("53b6e2bf-f0fc-43e3-ade4-25f3a74a42e1"_uuid);
        m_impl->graph->add_node("13514366-b0af-4a25-a4c6-384bd7277a35"_uuid);
    }

    void node_editor::update()
    {
        m_impl->update();
    }

    void node_editor::impl::update()
    {
        auto& io = ImGui::GetIO();

        const u32 nodeBackgroundColor = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);
        const u32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
        const u32 nodeBorderColor = ImGui::GetColorU32(ImGuiCol_Border);
        const u32 selectedNodeBorderColor = ImGui::GetColorU32(ImGuiCol_Text);
        const u32 gridColor = ImGui::GetColorU32(ImGuiCol_Border);

        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();

        ImGui::InvisibleButton("canvas",
            canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();
        const ImVec2 canvasPos = ImGui::GetItemRectMin();
        ImDrawList* const drawList = ImGui::GetWindowDrawList();

        // Handle panning
        if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            panOffset += io.MouseDelta;
        }

        // Handle zoom
        if (isHovered)
        {
            const f32 wheel = io.MouseWheel;

            if (wheel != 0.0f)
            {
                const ImVec2 mouseInCanvas = io.MousePos - canvasPos - panOffset;
                const f32 prev_zoom = zoom;
                zoom = ImClamp(zoom + wheel * g_ZoomSpeed, g_MinZoom, g_MaxZoom);
                panOffset -= mouseInCanvas * (zoom - prev_zoom);
            }
        }

        const ImVec2 origin = canvasPos + panOffset;

        // Draw grid
        const f32 gridStep = 64.f * zoom;

        for (f32 x = std::fmodf(panOffset.x, gridStep); x < canvasSize.x; x += gridStep)
        {
            drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                gridColor);
        }

        for (f32 y = std::fmodf(panOffset.y, gridStep); y < canvasSize.y; y += gridStep)
        {
            drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                gridColor);
        }

        dynamic_array<h32<node_graph_node>> nodes;
        graph->fetch_nodes(nodes);

        dynamic_array<h32<node_graph_in_pin>> inputPins;
        dynamic_array<h32<node_graph_out_pin>> outputPins;

        bool clickedOnAnyNode = false;

        // Draw nodes
        for (const h32 node : nodes)
        {
            // TODO: Clipping of nodes that are not visible

            const auto [posX, posY] = graph->get_ui_position(node);
            const ImVec2 pos{posX, posY};

            // TODO: Actually need to calculate the size properly
            const ImVec2 nodeSizeLogical{ImVec2(250, 350)};
            const ImVec2 nodeSizeScreen{nodeSizeLogical * zoom};

            const ImVec2 nodeScreenPos = world_to_screen(pos, origin);
            const ImVec2 nodeRectMin = nodeScreenPos;
            const ImVec2 nodeRectMax = nodeScreenPos + nodeSizeScreen;

            // Draw node body
            drawList->AddRectFilled(nodeScreenPos + ImVec2(0, g_TitleBarHeight * zoom),
                nodeRectMax,
                nodeBackgroundColor,
                g_NodeRounding,
                ImDrawFlags_RoundCornersBottom);

            // Draw title bar
            drawList->AddRectFilled(nodeRectMin,
                nodeScreenPos + ImVec2(nodeSizeScreen.x, g_TitleBarHeight * zoom),
                g_TitleBackground,
                g_NodeRounding,
                ImDrawFlags_RoundCornersTop);

            // Highlight selected nodes with a white border
            drawList->AddRect(nodeRectMin,
                nodeRectMax,
                selectedNode == node ? selectedNodeBorderColor : nodeBorderColor,
                g_NodeRounding);

            // We may want to have fonts available with different sizes, so it doesn't look as bad as it does when
            // zooming in/out
            ImFont* const font = ImGui::GetFont();
            const f32 fontSize = ImGui::GetFontSize() * zoom;

            cstring_view titleBarContent;

            const uuid& nodeType = graph->get_type(node);

            if (auto* const desc = graph->get_registry().find_node(nodeType))
            {
                titleBarContent = desc->name;
            }
            else
            {
                titleBarContent = "Unknown node";
            }

            drawList->AddText(font,
                fontSize,
                nodeScreenPos + g_TitleTextMargin * zoom,
                textColor,
                titleBarContent.c_str());

            inputPins.clear();
            outputPins.clear();

            graph->fetch_in_pins(node, inputPins);
            graph->fetch_out_pins(node, outputPins);

            constexpr f32 pinRadius = 4.0f;
            constexpr f32 pinTextMargin = 4.0f;
            constexpr f32 pinRowMargin = 4.0f;
            constexpr u32 inputColor = IM_COL32(200, 80, 80, 255);
            constexpr u32 outputColor = IM_COL32(80, 200, 100, 255);

            const f32 firstY = (g_TitleBarHeight + pinRowMargin);
            const f32 pinRowHeight = ImGui::GetFontSize() + pinRowMargin;

            {
                for (u32 i = 0; i < inputPins.size32(); ++i)
                {
                    const f32 y = firstY + pinRowHeight * i;

                    const ImVec2 pinScreenPos = nodeScreenPos + ImVec2(0, (y + .5f * pinRowHeight) * zoom);
                    drawList->AddCircleFilled(pinScreenPos, pinRadius * zoom, inputColor);

                    const h32 pin = inputPins[i];

                    const cstring_view name = graph->get_name(pin);

                    drawList->AddText(font,
                        fontSize,
                        pinScreenPos + ImVec2(pinRadius + pinTextMargin, -ImGui::GetFontSize() * .5f) * zoom,
                        textColor,
                        name.c_str());
                }
            }

            {
                for (u32 i = 0; i < outputPins.size32(); ++i)
                {
                    const f32 y = firstY + pinRowHeight * i;

                    const ImVec2 pinScreenPos =
                        nodeScreenPos + ImVec2(nodeSizeScreen.x, (y + .5f * pinRowHeight) * zoom);
                    drawList->AddCircleFilled(pinScreenPos, pinRadius * zoom, outputColor);

                    const h32 pin = outputPins[i];

                    const cstring_view name = graph->get_name(pin);
                    const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());

                    drawList->AddText(font,
                        fontSize,
                        pinScreenPos -
                            ImVec2(textSize.x + pinRadius + pinTextMargin, ImGui::GetFontSize() * .5f) * zoom,
                        textColor,
                        name.c_str());
                }
            }

            if (ImGui::IsMouseHoveringRect(nodeRectMin, nodeRectMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                // Start the node dragging, store the offset to set the frame of reference over multiple frames
                draggedNode = node;
                selectedNode = node;
                dragOffset = screen_to_world(io.MousePos, origin) - pos;
                clickedOnAnyNode = true;
            }
        }

        // Move node if dragging
        if (draggedNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const auto [newX, newY] = screen_to_world(io.MousePos, origin) - dragOffset;
            graph->set_ui_position(draggedNode, {newX, newY});
        }
        else if (!clickedOnAnyNode && isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            selectedNode = {};
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            draggedNode = {};
        }
    }
}