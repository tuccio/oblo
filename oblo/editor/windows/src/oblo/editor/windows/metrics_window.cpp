#include <oblo/editor/windows/metrics_window.hpp>

#include <oblo/core/platform/core.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/editor/service_context.hpp>
#include <oblo/editor/ui/property_table.hpp>
#include <oblo/editor/utility/data_inspector.hpp>
#include <oblo/editor/window_update_context.hpp>
#include <oblo/editor/windows/memory_usage_metrics.hpp>
#include <oblo/metrics/async_metrics.hpp>
#include <oblo/metrics/metrics.hpp>
#include <oblo/metrics/metrics_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/renderer/draw/resource_cache.hpp>
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
        metrics_module* metricsModule{};
        const property_registry* propertyRegistry{};
        const resource_cache* resourceCache{};
        metrics_state state = metrics_state::idle;
        dynamic_array<future<async_metrics>> pendingMetrics;
        dynamic_array<async_metrics> collectedMetrics;
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
                    metricsModule->stop_collecting();
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
                    pendingMetrics.clear();

                    metricsModule->stop_collecting();
                    metricsModule->collect_metrics(pendingMetrics);

                    bool allReady = true;

                    for (const auto& pending : pendingMetrics)
                    {
                        const expected e = pending.try_get_result();

                        if (!e.has_value())
                        {
                            if (e.error() == future_error::not_ready)
                            {
                                allReady = false;
                            }
                        }
                    }

                    if (allReady)
                    {
                        collectedMetrics.clear();

                        for (auto& m : pendingMetrics)
                        {
                            auto&& r = m.try_get_result();

                            if (r)
                            {
                                collectedMetrics.emplace_back(std::move(*r));
                            }
                        }

                        state = metrics_state::downloading;
                    }
                }
                else if (state == metrics_state::downloading)
                {
                    bool allDone = true;

                    for (auto& m : collectedMetrics)
                    {
                        if (!m.is_done())
                        {
                            m.update();
                            allDone = false;
                        }
                    }

                    if (allDone)
                    {

                        displayedMetrics.clear();

                        gather_global_metrics();

                        push_displayed_metrics(globalMetrics);

                        for (auto& m : collectedMetrics)
                        {
                            push_displayed_metrics(m.get_entries());
                        }

                        collectedMetrics.clear();
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
            metricsModule->start_collecting();
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

        void push_displayed_metrics(std::span<const async_metrics_entry> entries)
        {
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
        }
    };

    metrics_window::metrics_window() = default;
    metrics_window::~metrics_window() = default;

    bool metrics_window::init(const window_update_context& ctx)
    {
        m_impl = allocate_unique<impl>();
        m_impl->metricsModule = module_manager::get().find<metrics_module>();
        m_impl->propertyRegistry = ctx.services.find<const property_registry>();
        auto* const reflection = ctx.services.find<const reflection::reflection_registry>();

        if (!reflection || !m_impl->metricsModule || !m_impl->propertyRegistry)
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