#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <oblo/core/deque.hpp>
#include <oblo/core/handle_flat_pool_set.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2i.hpp>
#include <oblo/nodes/editor/node_editor.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_graph_registry.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
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

        constexpr u32 g_EdgeColor = IM_COL32(255, 255, 0, 255);
        constexpr u32 g_TitleBackground{IM_COL32(80, 130, 200, 255)};

        enum class draw_list_channel : u8
        {
            edges,
            nodes,
            enum_max,
        };

        enum class graph_element : u8
        {
            nil,
            node,
            in_pin,
            out_pin,
        };

        struct node_ui_data
        {
            u64 zOrder;

            ImVec2 screenPosition;
            ImVec2 screenSize;
        };

        f32 calculate_pin_y_offset(f32 y, f32 pinRowHeight, f32 zoom)
        {
            return (y + .5f * pinRowHeight) * zoom;
        }
    }

    struct node_editor::impl
    {
        void update();

        void draw_edge(ImDrawList& drawList, const ImVec2& src, const ImVec2& dst, const oblo::u32 edgeColor) const;

        ImVec2 logical_to_screen(const ImVec2& logical, const ImVec2& origin) const noexcept
        {
            return origin + logical * zoom;
        }

        ImVec2 screen_to_logical(const ImVec2& logical, const ImVec2& origin) const noexcept
        {
            return (logical - origin) / zoom;
        }

        void reset_drag_source()
        {
            dragSource = graph_element::nil;
            draggedNode = {};
        }

        void set_drag_source(h32<node_graph_node> source)
        {
            OBLO_ASSERT(source);
            dragSource = graph_element::node;
            draggedNode = source;
        }

        void set_drag_source(h32<node_graph_in_pin> source)
        {
            OBLO_ASSERT(source);
            dragSource = graph_element::in_pin;
            draggedInPin = source;
        }

        void set_drag_source(h32<node_graph_out_pin> source)
        {
            OBLO_ASSERT(source);
            dragSource = graph_element::out_pin;
            draggedOutPin = source;
        }

        void select_node(h32<node_graph_node> node)
        {
            selectedNode = node;
            nodeUiData.at(node).zOrder = increment_z_order();
        }

        void reset_selection()
        {
            selectedNode = {};
        }

        u64 increment_z_order()
        {
            return ++nextZOrder;
        }

        u64 nextZOrder{};
        node_graph* graph{};
        ImVec2 panOffset{0.f, 0.f};
        ImVec2 dragOffset{0.f, 0.f};
        f32 zoom{1.f};

        graph_element dragSource{graph_element::nil};

        union {
            h32<node_graph_node> draggedNode;
            h32<node_graph_out_pin> draggedOutPin;
            h32<node_graph_in_pin> draggedInPin;
        };

        h32<node_graph_node> selectedNode{};

        h32_flat_extpool_dense_map<node_graph_node, node_ui_data> nodeUiData;

        ImDrawListSplitter drawListSplitter;
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

        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("canvas",
            canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();
        const ImVec2 canvasPos = ImGui::GetItemRectMin();
        ImDrawList* const drawList = ImGui::GetWindowDrawList();

        drawListSplitter.Clear();
        drawListSplitter.Split(drawList, i32(draw_list_channel::enum_max));

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

        // Should maybe increase it to avoid running GC too often
        constexpr u32 gcThreshold = 0;

        const bool shouldGC = nodes.size() > gcThreshold + nodeUiData.size();

        h32_flat_extpool_dense_set<node_graph_node> gcSet;

        if (shouldGC)
        {
            gcSet.reserve_dense(nodeUiData.size());
            gcSet.reserve_sparse(nodeUiData.size());

            for (const h32 node : nodeUiData.keys())
            {
                gcSet.emplace(node);
            }
        }

        // Add all new nodes to the nodeUiData map, calculate new screenPositions
        for (const h32 node : nodes)
        {
            const auto [it, inserted] = nodeUiData.emplace(node);

            if (inserted)
            {
                it->zOrder = increment_z_order();
            }
            else if (shouldGC)
            {
                gcSet.erase(node);
            }

            const auto [posX, posY] = graph->get_ui_position(node);
            const ImVec2 pos{posX, posY};

            // TODO: Actually need to calculate the size properly
            const ImVec2 nodeSizeLogical{ImVec2(250, 350)};

            it->screenPosition = logical_to_screen(pos, origin);
            it->screenSize = nodeSizeLogical * zoom;
        }

        if (shouldGC)
        {
            for (const h32 removedNode : gcSet.keys())
            {
                nodeUiData.erase(removedNode);
            }
        }

        dynamic_array<h32<node_graph_in_pin>> inputPins;
        dynamic_array<h32<node_graph_out_pin>> outputPins;

        bool clickedOnAnyNode = false;
        graph_element mouseHoveringElement = graph_element::nil;

        ImVec2 hoveredPinScreenPos;

        // Sort by Z order before drawing
        std::sort(nodes.begin(),
            nodes.end(),
            [this](const h32<node_graph_node> lhs, const h32<node_graph_node> rhs)
            { return nodeUiData.at(lhs).zOrder < nodeUiData.at(rhs).zOrder; });

        // Settings for the looks and positioning of pins
        constexpr f32 pinInvisibleButtonPadding = 1.0f;
        constexpr f32 pinRadius = 5.0f;
        constexpr f32 pinTextMargin = 4.0f;
        constexpr f32 pinRowMargin = 4.0f;
        constexpr u32 inputColor = IM_COL32(200, 80, 80, 255);
        constexpr u32 outputColor = IM_COL32(80, 200, 100, 255);

        const f32 firstY = (g_TitleBarHeight + pinRowMargin);
        const f32 pinRowHeight = ImGui::GetFontSize() + pinRowMargin;

        // Draw nodes
        drawListSplitter.SetCurrentChannel(drawList, i32(draw_list_channel::nodes));

        for (const h32 node : nodes)
        {
            // TODO: Clipping of nodes that are not visible
            const auto& uiData = nodeUiData.at(node);

            const ImVec2 nodeScreenPos = uiData.screenPosition;
            const ImVec2 nodeScreenSize = uiData.screenSize;
            const ImVec2 nodeRectMin = nodeScreenPos;
            const ImVec2 nodeRectMax = nodeScreenPos + nodeScreenSize;

            // Draw node body
            drawList->AddRectFilled(nodeScreenPos + ImVec2(0, g_TitleBarHeight * zoom),
                nodeRectMax,
                nodeBackgroundColor,
                g_NodeRounding,
                ImDrawFlags_RoundCornersBottom);

            // Draw title bar
            drawList->AddRectFilled(nodeRectMin,
                nodeScreenPos + ImVec2(nodeScreenSize.x, g_TitleBarHeight * zoom),
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

            string_builder stringBuilder;

            bool inputConsumed = false;

            const f32 pinInvisibleButtonSize = max(1.f, (pinInvisibleButtonPadding + pinRadius * 2) * zoom);
            const ImVec2 pinInvisibleButtonSize2d{pinInvisibleButtonSize, pinInvisibleButtonSize};

            // Draw input pins

            inputPins.clear();
            graph->fetch_in_pins(node, inputPins);

            for (u32 i = 0; i < inputPins.size32(); ++i)
            {
                const h32 pin = inputPins[i];

                const f32 y = firstY + pinRowHeight * i;
                const ImVec2 pinScreenPos = nodeScreenPos + ImVec2{0.f, calculate_pin_y_offset(y, pinRowHeight, zoom)};
                drawList->AddCircleFilled(pinScreenPos, pinRadius * zoom, inputColor);

                // Use an invisible button for input pin
                ImGui::SetCursorScreenPos(pinScreenPos - ImVec2(pinRadius * zoom, pinRadius * zoom));

                const bool isPressed = ImGui::InvisibleButton(stringBuilder.format("##ipin{}", pin.value).c_str(),
                    ImVec2(pinRadius * 2 * zoom, pinRadius * 2 * zoom),
                    ImGuiButtonFlags_PressedOnClick);

                // Handle dragging inputs
                if (isPressed)
                {
                    dragOffset = screen_to_logical(pinScreenPos, origin);
                    set_drag_source(pin);
                    inputConsumed = true;

                    // Clear input on click regardless
                    graph->clear_connected_output(pin);
                }
                else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                {
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        // If we were dragging an edge, connect it
                        if (dragSource == graph_element::out_pin)
                        {
                            graph->clear_connected_output(pin);
                            graph->connect(draggedOutPin, pin);
                        }

                        reset_drag_source();
                    }

                    mouseHoveringElement = graph_element::in_pin;
                    hoveredPinScreenPos = pinScreenPos;
                }

                const cstring_view name = graph->get_name(pin);

                drawList->AddText(font,
                    fontSize,
                    pinScreenPos + ImVec2(pinRadius + pinTextMargin, -ImGui::GetFontSize() * .5f) * zoom,
                    textColor,
                    name.c_str());

                if (const auto srcPin = graph->get_connected_output(pin))
                {
                    const auto srcNode = graph->get_owner_node(srcPin);

                    const auto& srcNodeUiData = nodeUiData.at(srcNode);
                    const auto& dstNodeUiData = nodeUiData.at(node);

                    const ImVec2 srcNodeScreenPos = srcNodeUiData.screenPosition;
                    const ImVec2 dstNodeScreenPos = dstNodeUiData.screenPosition;

                    const f32 dstY = firstY + pinRowHeight * i;

                    const ImVec2 dstPinScreenPos =
                        dstNodeScreenPos + ImVec2{0.f, calculate_pin_y_offset(dstY, pinRowHeight, zoom)};

                    // Find the pin index at the source to calculate
                    outputPins.clear();
                    graph->fetch_out_pins(srcNode, outputPins);

                    u32 srcIndex = 0;

                    for (u32 j = 0; j < outputPins.size32(); ++j)
                    {
                        if (outputPins[j] == srcPin)
                        {
                            srcIndex = j;
                            break;
                        }
                    }

                    const f32 srcY = firstY + pinRowHeight * srcIndex;

                    const ImVec2 srcPinScreenPos = srcNodeScreenPos +
                        ImVec2{srcNodeUiData.screenSize.x, calculate_pin_y_offset(srcY, pinRowHeight, zoom)};

                    // TODO: Clipping of the edge
                    drawListSplitter.SetCurrentChannel(drawList, i32(draw_list_channel::edges));
                    draw_edge(*drawList, srcPinScreenPos, dstPinScreenPos, g_EdgeColor);
                    drawListSplitter.SetCurrentChannel(drawList, i32(draw_list_channel::nodes));
                }
            }

            // Draw output pins

            outputPins.clear();
            graph->fetch_out_pins(node, outputPins);

            drawListSplitter.SetCurrentChannel(drawList, i32(draw_list_channel::nodes));

            for (u32 i = 0; i < outputPins.size32(); ++i)
            {
                const h32 pin = outputPins[i];

                const f32 y = firstY + pinRowHeight * i;
                const ImVec2 pinScreenPos =
                    nodeScreenPos + ImVec2{nodeScreenSize.x, calculate_pin_y_offset(y, pinRowHeight, zoom)};
                drawList->AddCircleFilled(pinScreenPos, pinRadius * zoom, outputColor);

                // Use an invisible button for output pin
                ImGui::SetCursorScreenPos(pinScreenPos - ImVec2(pinRadius * zoom, pinRadius * zoom));
                const bool isPressed = ImGui::InvisibleButton(stringBuilder.format("##opin{}", pin.value).c_str(),
                    ImVec2(pinRadius * 2 * zoom, pinRadius * 2 * zoom),
                    ImGuiButtonFlags_PressedOnClick);

                const cstring_view name = graph->get_name(pin);
                const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());

                drawList->AddText(font,
                    fontSize,
                    pinScreenPos - ImVec2(textSize.x + pinRadius + pinTextMargin, ImGui::GetFontSize() * .5f) * zoom,
                    textColor,
                    name.c_str());

                // Handle dragging outputs
                if (isPressed)
                {
                    dragOffset = screen_to_logical(pinScreenPos, origin);
                    set_drag_source(pin);
                    inputConsumed = true;
                }
                else if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                {
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        // If we were dragging an edge, connect it
                        if (dragSource == graph_element::in_pin)
                        {
                            graph->clear_connected_output(draggedInPin);
                            graph->connect(pin, draggedInPin);
                        }

                        reset_drag_source();
                    }

                    mouseHoveringElement = graph_element::out_pin;
                    hoveredPinScreenPos = pinScreenPos;
                }
            }

            if (!inputConsumed && ImGui::IsMouseHoveringRect(nodeRectMin, nodeRectMax) &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                // Start the node dragging, store the offset to set the frame of reference over multiple frames
                set_drag_source(node);
                select_node(node);

                const vec2 uiPosition = graph->get_ui_position(node);
                dragOffset = screen_to_logical(io.MousePos, origin) - ImVec2{uiPosition.x, uiPosition.y};
                clickedOnAnyNode = true;
            }
        }

        // Move node if dragging
        if (dragSource == graph_element::node && draggedNode && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const auto [newX, newY] = screen_to_logical(io.MousePos, origin) - dragOffset;
            graph->set_ui_position(draggedNode, {newX, newY});
        }
        else if (!clickedOnAnyNode && isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            reset_selection();
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            reset_drag_source();
        }

        // While dragging, update the dragged edge that will be draw next frame
        if (dragSource == graph_element::out_pin || dragSource == graph_element::in_pin)
        {
            const ImVec2 dragSourceScreen = logical_to_screen(dragOffset, origin);

            ImVec2 curveEndPos = io.MousePos;

            if (mouseHoveringElement != graph_element::nil && dragSource != mouseHoveringElement)
            {
                curveEndPos = hoveredPinScreenPos;
            }

            drawListSplitter.SetCurrentChannel(drawList, i32(draw_list_channel::edges));
            draw_edge(*drawList, dragSourceScreen, curveEndPos, g_EdgeColor);
        }

        drawListSplitter.Merge(drawList);
    }

    void node_editor::impl::draw_edge(
        ImDrawList& drawList, const ImVec2& src, const ImVec2& dst, const oblo::u32 edgeColor) const
    {
        constexpr f32 lineWidth = 2.f;

        const f32 cpOffset = src.x < dst.x ? 64.f : -64.f;

        const ImVec2 p0 = src;
        const ImVec2 p1 = src + ImVec2(cpOffset * zoom, 0);
        const ImVec2 p2 = dst - ImVec2(cpOffset * zoom, 0);
        const ImVec2 p3 = dst;

        drawList.AddBezierCubic(p0, p1, p2, p3, edgeColor, lineWidth);
    }
}