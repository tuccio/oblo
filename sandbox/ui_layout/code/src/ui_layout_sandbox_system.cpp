#include <ui_layout_sandbox_system.hpp>

#include <ui_layout_graph.hpp>
#include <ui_layout_render_node.hpp>

#include <oblo/app/graphics_app.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/input/input_event.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/log/log.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/renderer/graph/frame_graph.hpp>
#include <oblo/renderer/renderer.hpp>
#include <oblo/ui/ui.hpp>

#include <span>

namespace oblo
{
    namespace
    {
        [[maybe_unused]] ui::animation_config make_fade_scale_animation(time duration)
        {
            using namespace oblo::ui;

            return {
                .duration = duration,
                .easing = easing_function::ease_out,
                .properties = bounding_box_properties | animation_property::background_color,
                .enter =
                    {
                        .setInitialState =
                            [](const animated_values& target, animation_properties)
                        {
                            animated_values out = target;
                            out.boundingBox.width *= 0.85f;
                            out.boundingBox.height *= 0.85f;
                            out.backgroundColor.a = 0.f;
                            return out;
                        },
                    },
                .exit =
                    {
                        .setFinalState =
                            [](const animated_values& initial, animation_properties)
                        {
                            animated_values out = initial;
                            out.backgroundColor.a = 0.f;
                            return out;
                        },
                    },
            };
        }
    }

    ui_layout_sandbox_system::ui_layout_sandbox_system() = default;

    ui_layout_sandbox_system::~ui_layout_sandbox_system()
    {
        if (m_renderer && m_graph)
        {
            m_renderer->get_frame_graph().remove(m_graph);
        }

        m_atlasCache.clear();
        m_ui.shutdown();
        m_app.shutdown();
    }

    void ui_layout_sandbox_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<renderer>();
        OBLO_ASSERT(m_renderer);

        m_nodeRegistry.register_node<ui_layout_render_node>();
        m_graphTemplate = ui_layout_view::create(m_nodeRegistry);

        if (!m_app.init({.title = "UI Layout Sandbox", .windowWidth = 1280, .windowHeight = 720}) || !m_ui.init())
        {
            log::error("Failed to create the UI Layout sandbox window");
            return;
        }

        m_app.set_input_queue(&m_inputQueue);

        auto& frameGraph = m_renderer->get_frame_graph();

        m_graph = frameGraph.instantiate(m_graphTemplate);
        m_app.set_output(m_graph, ui_layout_view::OutLayoutImage);

        m_active = true;

        update(ctx);
    }

    void ui_layout_sandbox_system::update(const ecs::system_update_context& ctx)
    {
        using namespace oblo::ui;

        if (!m_active)
        {
            return;
        }

        if (!m_app.process_events())
        {
            if (m_graph)
            {
                m_renderer->get_frame_graph().remove(m_graph);
                m_graph = {};
            }

            m_app.shutdown();
            m_active = false;
            return;
        }

        auto& frameGraph = m_renderer->get_frame_graph();

        const auto windowSize = m_app.get_main_window().get_size();
        const vec2u resolution{max(windowSize.x, 1u), max(windowSize.y, 1u)};

        const auto& events = m_inputQueue.get_events();

        const vec2 size{f32(resolution.x), f32(resolution.y)};

        m_ui.begin_frame({events.data(), events.size()}, ctx.dt, size);

        m_inputQueue.clear();

        {
            auto root = panel(m_ui,
                {1},
                panel_style{
                    .backgroundColor = {0.12f, 0.12f, 0.16f, 1.f},
                    .cornerRadius = 6.f,
                    .padding = {16.f, 16.f, 16.f, 16.f},
                    .direction = layout_direction::top_to_bottom,
                    .gap = 16.f,
                    .width = percent_size(1.f),
                    .height = percent_size(1.f),
                });

            {
                auto header = panel(m_ui,
                    {2},
                    panel_style{
                        .padding = {8.f, 8.f, 8.f, 8.f},
                        .direction = layout_direction::left_to_right,
                        .gap = 8.f,
                        .width = percent_size(1.f),
                        .height = fit_size(),
                        .alignment = alignment::center_left(),
                    });

                string_builder nameBuilder;

                for (u32 i = 0; i < 4; ++i)
                {
                    nameBuilder.clear().format("Button #{}", i);
                    button(m_ui, {100 + i}, nameBuilder.as<hashed_string_view>());
                }

                checkbox(m_ui, {700}, m_showAnimatedPanel, "Show panel"_hsv);
                checkbox(m_ui, {701}, m_expandedAnimatedPanel, "Expand"_hsv);
            }

            {
                auto middle = panel(m_ui,
                    {3},
                    panel_style{
                        .padding = {8.f, 8.f, 8.f, 8.f},
                        .direction = layout_direction::left_to_right,
                        .gap = 16.f,
                        .width = percent_size(1.f),
                        .height = percent_size(0.65f),
                    });

                {
                    auto sidebar = panel(m_ui,
                        {4},
                        panel_style{
                            .direction = layout_direction::top_to_bottom,
                            .gap = 8.f,
                            .width = percent_size(0.25f),
                            .height = percent_size(1.f),
                        });

                    string_builder nameBuilder;

                    for (u32 i = 0; i < array_size(m_sidebarChecked); ++i)
                    {
                        nameBuilder.clear().format("Item #{}", i);
                        bool& checked = m_sidebarChecked[i];
                        checkbox(m_ui, {200 + i}, checked, nameBuilder.as<hashed_string_view>());
                    }
                }

                if (m_showAnimatedPanel)
                {
                    const sizing panelWidth = m_expandedAnimatedPanel ? percent_size(0.6f) : percent_size(0.3f);
                    const sizing panelHeight = m_expandedAnimatedPanel ? percent_size(0.9f) : percent_size(0.5f);

                    auto animatedPanel = panel(m_ui,
                        {800},
                        panel_style{
                            .backgroundColor = {0.08f, 0.08f, 0.10f, 1.f},
                            .cornerRadius = 12.f,
                            .padding = {16.f, 16.f, 16.f, 16.f},
                            .direction = layout_direction::top_to_bottom,
                            .gap = 8.f,
                            .width = panelWidth,
                            .height = panelHeight,
                            .animation = make_fade_scale_animation(time::from_seconds(0.5f)),
                        });

                    label(m_ui, {801}, m_expandedAnimatedPanel ? "Expanded panel"_hsv : "Animated panel"_hsv);
                }
            }

            {
                auto footer = panel(m_ui,
                    {6},
                    panel_style{
                        .padding = {8.f, 8.f, 8.f, 8.f},
                        .direction = layout_direction::left_to_right,
                        .gap = 8.f,
                        .width = percent_size(1.f),
                        .height = ui::fit_size(),
                    });

                button(m_ui, {600}, "Apply"_hsv);

                const bool close = button(m_ui, {601}, "Close"_hsv);

                if (close)
                {
                    m_app.get_main_window().destroy();
                }
            }
        }

        m_ui.end_frame();

        const span drawCommands = m_ui.get_draw_commands();
        const span textures = m_ui.get_textures();
        const span textureCommands = m_ui.get_texture_commands();

        frameGraph.set_input(m_graph, ui_layout_view::InResolution, resolution).assert_value();
        frameGraph.set_input(m_graph, ui_layout_view::InDrawCommands, drawCommands).assert_value();
        frameGraph.set_input(m_graph, ui_layout_view::InTextures, textures).assert_value();
        frameGraph.set_input(m_graph, ui_layout_view::InTextureCommands, textureCommands).assert_value();
        frameGraph.set_input(m_graph, ui_layout_view::InAtlasCache, &m_atlasCache).assert_value();

        ++m_frameIndex;
    }
}
