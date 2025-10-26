#include <oblo/editor/windows/metrics_window.hpp>

#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/editor/utility/data_inspector.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/metrics/async_metrics.hpp>
#include <oblo/metrics/metrics.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/vulkan/graph/frame_graph.hpp>

#include <imgui.h>

namespace oblo::editor
{
    namespace
    {
        enum class metrics_state : u8
        {
            idle,
            pending,
            downloading,
        };

        struct displayed_metric
        {
            metrics_entry entry;
            property_tree tree;
        };
    }

    struct metrics_window::impl
    {
        vk::frame_graph* frameGraph{};
        const property_registry* propertyRegistry{};
        metrics_state state = metrics_state::idle;
        future<async_metrics> pendingMetrics;
        async_metrics asyncMetrics;
        dynamic_array<displayed_metric> displayedMetrics;
        data_inspector inspector;
        bool keepRecording{true};

        void draw()
        {
            constexpr const char* labelStart = "Record Metrics";
            constexpr const char* labelCancel = "Cancel";

            const ImVec2 labelSizes[] = {
                ImGui::CalcTextSize(labelStart),
                ImGui::CalcTextSize(labelCancel),
            };

            auto& style = ImGui::GetStyle();

            ImVec2 buttonSize;
            buttonSize.x = max(labelSizes[0].x, labelSizes[1].x) + style.FramePadding.x * 2;
            buttonSize.y = max(labelSizes[0].y, labelSizes[1].y) + style.FramePadding.y * 2;

            if (state == metrics_state::idle)
            {
                if (ImGui::Button(labelStart, buttonSize))
                {
                    request_metrics();
                }
            }
            else
            {
                if (ImGui::Button(labelCancel, buttonSize))
                {
                    pendingMetrics.reset();
                    state = metrics_state::idle;
                }
            }

            ImGui::SameLine();
            ImGui::Checkbox("Continuous recording", &keepRecording);

            ImGui::Separator();

            inspector.begin();
            string_builder builder;

            for (auto& metricEntry : displayedMetrics)
            {
                builder = metricEntry.entry.type.name;

                if (ImGui::CollapsingHeader(builder.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::BeginDisabled();
                    inspector.build_property_table(metricEntry.tree, metricEntry.entry.data.data());
                    ImGui::EndDisabled();
                }
            }

            inspector.end();
        }

        void update_state()
        {
            if (state != metrics_state::idle)
            {
                if (state == metrics_state::pending)
                {
                    const expected e = pendingMetrics.try_get_result();

                    if (!e.has_value())
                    {
                        if (e.error() == future_error::not_ready)
                        {
                            state = metrics_state::downloading;
                        }
                        else
                        {
                            // Maybe report error instead?
                            state = metrics_state::idle;
                        }
                    }
                    else
                    {
                        asyncMetrics = std::move(e.value());
                        state = metrics_state::downloading;
                    }
                }
                else if (state == metrics_state::downloading)
                {
                    asyncMetrics.update();

                    if (asyncMetrics.is_done())
                    {
                        const std::span entries = asyncMetrics.get_entries();

                        displayedMetrics.clear();
                        displayedMetrics.reserve(entries.size());

                        for (const auto& entry : entries)
                        {
                            const expected e = entry.download.try_get_result();

                            if (e.has_value())
                            {
                                auto& newEntry = displayedMetrics.emplace_back();

                                newEntry.entry.type = entry.type;
                                newEntry.entry.data = std::move(e.value());

                                propertyRegistry->try_build_from_reflection(newEntry.tree, entry.type);
                            }
                        }

                        asyncMetrics = {};
                        state = metrics_state::idle;

                        if (keepRecording)
                        {
                            request_metrics();
                        }
                    }
                }
            }
        }

        void request_metrics()
        {
            state = metrics_state::pending;
            pendingMetrics = frameGraph->request_metrics();
        }
    };

    metrics_window::metrics_window() = default;
    metrics_window::~metrics_window() = default;

    bool metrics_window::init(const window_update_context& ctx)
    {
        m_impl = allocate_unique<impl>();
        m_impl->frameGraph = ctx.services.find<vk::frame_graph>();
        m_impl->propertyRegistry = ctx.services.find<const property_registry>();
        auto* const reflection = ctx.services.find<const reflection::reflection_registry>();

        if (!reflection || !m_impl->frameGraph || !m_impl->propertyRegistry)
        {
            return false;
        }

        m_impl->inspector.init(reflection, nullptr);

        return true;
    }

    bool metrics_window::update(const window_update_context&)
    {
        bool isOpen = true;

        if (ImGui::Begin("Metrics Monitor", &isOpen))
        {
            m_impl->update_state();
            m_impl->draw();
        }

        ImGui::End();
        return isOpen;
    }
}