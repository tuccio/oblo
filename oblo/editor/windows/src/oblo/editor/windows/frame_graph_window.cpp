#include <oblo/editor/windows/frame_graph_window.hpp>

#include <oblo/app/imgui_texture.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/texture.hpp>

#include <imgui.h>

namespace oblo::editor
{
    struct frame_graph_window::impl
    {
        vk::frame_graph* frameGraph{};

        deque<h32<vk::frame_graph_subgraph>> subgraphs;
        deque<vk::frame_graph_output_desc> subgraphOutputs;

        h32<vk::frame_graph_subgraph> selectedGraph{};
        u32 selectedOutputIndex{};

        type_id selectedOutputType{};
        string selectedOutputName;

        void draw_left_panel();
        void draw_right_panel();
    };

    frame_graph_window::frame_graph_window() = default;

    frame_graph_window::~frame_graph_window() = default;

    void frame_graph_window::init(const window_update_context& ctx)
    {
        m_impl = allocate_unique<impl>();
        m_impl->frameGraph = ctx.services.find<vk::frame_graph>();
        OBLO_ASSERT(m_impl->frameGraph);
    }

    bool frame_graph_window::update(const window_update_context&)
    {
        bool isOpen = true;

        if (ImGui::Begin("Frame Graph Debug", &isOpen))
        {
            m_impl->draw_left_panel();
            ImGui::SameLine();
            m_impl->draw_right_panel();
        }

        ImGui::End();

        return isOpen;
    }

    void frame_graph_window::impl::draw_left_panel()
    {
        if (!ImGui::BeginChild("#left_panel", ImVec2{300, 0}, ImGuiChildFlags_ResizeX))
        {
            ImGui::EndChild();
            return;
        }

        subgraphs.clear();
        frameGraph->fetch_subgraphs(subgraphs);

        string_builder builder;

        for (auto& sg : subgraphs)
        {
            constexpr i32 sgNodeFlags =
                ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;

            builder.clear().format("Subgraph #{}", sg.value);
            const bool expanded = ImGui::TreeNodeEx(builder.c_str(), sgNodeFlags);

            if (expanded)
            {
                subgraphOutputs.clear();
                frameGraph->fetch_outputs(sg, subgraphOutputs);

                for (u32 outputIndex = 0; outputIndex < subgraphOutputs.size32(); ++outputIndex)
                {
                    i32 outNodeFlags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf;

                    const bool isSelected = selectedGraph == sg && selectedOutputIndex == outputIndex;

                    if (isSelected)
                    {
                        outNodeFlags |= ImGuiTreeNodeFlags_Selected;
                    }

                    const auto& output = subgraphOutputs[outputIndex];

                    builder = output.name;
                    ImGui::TreeNodeEx(builder.c_str(), outNodeFlags);

                    if (ImGui::IsItemActivated())
                    {
                        selectedGraph = sg;
                        selectedOutputIndex = outputIndex;
                        selectedOutputName = output.name;
                        selectedOutputType = output.type;
                    }

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
        }

        ImGui::EndChild();
    }

    void frame_graph_window::impl::draw_right_panel()
    {
        if (!ImGui::BeginChild("#right_panel", ImVec2{0, 0}, 0, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::EndChild();
            return;
        }

        if (selectedOutputType == get_type_id<vk::texture>())
        {
            const auto imageId = imgui::add_image(selectedGraph, selectedOutputName);

            ImVec2 imageSize{};

            if (auto t = frameGraph->get_output<vk::texture>(selectedGraph, selectedOutputName))
            {
                const auto [w, h, d] = (*t)->initializer.extent;
                imageSize = {f32(w), f32(h)};
            }

            if (imageSize.x == 0 || imageSize.y == 0)
            {
                imageSize = ImGui::GetContentRegionAvail();
            }

            ImGui::Image(imageId, imageSize);
        }

        ImGui::EndChild();
    }
}