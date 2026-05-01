#include <oblo/editor/windows/metrics_window.hpp>

#include <oblo/core/iterator/concat_range.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/editor/utility/data_inspector.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/memory_usage_metrics.hpp>
#include <oblo/metrics/async_metrics.hpp>
#include <oblo/metrics/metrics.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/renderer/draw/resource_cache.hpp>
#include <oblo/renderer/graph/frame_graph.hpp>
#include <oblo/renderer/renderer.hpp>

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
        frame_graph* frameGraph{};
        const property_registry* propertyRegistry{};
        const resource_cache* resourceCache{};
        metrics_state state = metrics_state::idle;
        future<async_metrics> pendingFrameGraphMetrics;
        async_metrics frameGraphMetrics;
        dynamic_array<async_metrics::entry> globalMetrics;
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
                    pendingFrameGraphMetrics.reset();
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
                    const expected fg = pendingFrameGraphMetrics.try_get_result();

                    if (!fg.has_value())
                    {
                        if (fg.error() == future_error::not_ready)
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
                        frameGraphMetrics = std::move(fg.value());
                        state = metrics_state::downloading;
                    }
                }
                else if (state == metrics_state::downloading)
                {
                    frameGraphMetrics.update();

                    if (frameGraphMetrics.is_done())
                    {
                        const std::span frameGraphEntries = frameGraphMetrics.get_entries();

                        displayedMetrics.clear();
                        displayedMetrics.reserve(frameGraphEntries.size());

                        gather_global_metrics();

                        for (const auto& entry : concat_range(globalMetrics, frameGraphEntries))
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

                        frameGraphMetrics = {};
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
            pendingFrameGraphMetrics = frameGraph->request_metrics();
        }

        void gather_global_metrics()
        {
            globalMetrics.clear();

            memory_usage_metrics memoryUsage{
                .ram = platform::get_ram_usage().value_or(0),

            };

            if (resourceCache)
            {
                memoryUsage.vramTextureResources = resourceCache->calculate_texture_usage();
            }

            set_metrics_data_sync<memory_usage_metrics>(globalMetrics.emplace_back(), memoryUsage);
        }
    };

    metrics_window::metrics_window() = default;
    metrics_window::~metrics_window() = default;

    bool metrics_window::init(const window_update_context& ctx)
    {
        m_impl = allocate_unique<impl>();
        m_impl->frameGraph = ctx.services.find<frame_graph>();
        m_impl->propertyRegistry = ctx.services.find<const property_registry>();
        auto* const reflection = ctx.services.find<const reflection::reflection_registry>();

        if (!reflection || !m_impl->frameGraph || !m_impl->propertyRegistry)
        {
            return false;
        }

        m_impl->inspector.init(reflection, nullptr);

        auto* const r = ctx.services.find<renderer>();

        if (r)
        {
            m_impl->resourceCache = &r->get_resource_cache();
        }

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