#include <oblo/app/graphics_app.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/finally.hpp>
#include <oblo/core/platform/core.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/time/clock.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/input/input_event.hpp>
#include <oblo/input/input_queue.hpp>
#include <oblo/log/log.hpp>
#include <oblo/log/log_module.hpp>
#include <oblo/log/sinks/file_sink.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/renderer/graph/frame_graph_registry.hpp>
#include <oblo/renderer/graph/frame_graph_template.hpp>
#include <oblo/renderer/renderer.hpp>
#include <oblo/ui/ui.hpp>
#include <oblo/vulkan/vulkan_engine_module.hpp>

#include <ui_layout_atlas_cache.hpp>
#include <ui_layout_graph.hpp>
#include <ui_layout_render_node.hpp>

namespace oblo::ui
{
    namespace
    {
        // Stable layout ids. Grouping them here avoids magic numbers and accidental
        // collisions between sibling elements (ids must be unique per frame).
        constexpr u32 rootId = 1;
        constexpr u32 headerId = 2;
        constexpr u32 middleId = 3;
        constexpr u32 footerId = 6;

        constexpr u32 firstButtonId = 100;
        constexpr u32 numHeaderButtons = 4;

        constexpr u32 showPanelId = 700;
        constexpr u32 expandPanelId = 701;

        constexpr u32 firstSidebarItemId = 200;

        constexpr u32 animatedPanelId = 800;
        constexpr u32 animatedLabelId = 801;

        constexpr u32 applyId = 600;
        constexpr u32 closeId = 601;
        constexpr u32 statusId = 900;

        constexpr u32 comboEasingId = 3000;
        constexpr u32 radioDirectionId = 3200;
        constexpr layout_id dirVerticalId{radioDirectionId + 1u};
        constexpr layout_id dirHorizontalId{radioDirectionId + 2u};
        constexpr u32 sliderCornerId = 3300;

        constexpr hashed_string_view easing_names[] = {
            "Linear"_hsv,
            "Ease In"_hsv,
            "Ease Out"_hsv,
            "Ease In-Out"_hsv,
            "Ease In Back"_hsv,
            "Ease Out Back"_hsv,
            "Ease Out Elastic"_hsv,
            "Ease Out Bounce"_hsv,
        };

        ui::animation_config make_fade_scale_animation(time duration,
            easing_function easing = easing_function::ease_out)
        {
            return {
                .duration = duration,
                .easing = easing,
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

    // A self-contained UI sandbox: it boots oblo's graphics engine + renderer on its own,
    // opens a dedicated window and renders the UI layout directly through a frame-graph
    // subgraph. No ECS world, no module registration, no editor required.
    class ui_layout_sandbox
    {
    public:
        bool init(vk::vulkan_engine_module& engine)
        {
            m_renderer = &engine.get_renderer();

            m_nodeRegistry.register_node<ui_layout_render_node>();
            m_graphTemplate = ui_layout_view::create(m_nodeRegistry);

            if (!m_app.init({.title = "UI Layout Sandbox", .windowWidth = 1280, .windowHeight = 720}) || !m_ui.init())
            {
                log::error("Failed to create the UI Layout sandbox window");
                return false;
            }

            m_app.set_input_queue(&m_inputQueue);

            auto& frameGraph = m_renderer->get_frame_graph();

            m_graph = frameGraph.instantiate(m_graphTemplate);
            m_app.set_output(m_graph, ui_layout_view::OutLayoutImage);

            return true;
        }

        void run()
        {
            time lastTime = clock::now();

            while (m_app.process_events())
            {
                const time now = clock::now();
                const time dt = now - lastTime;
                lastTime = now;

                if (!m_app.acquire_images())
                {
                    continue;
                }

                build_frame(to_f32_seconds(dt));

                m_app.present();
                m_inputQueue.clear();

                ++m_frameIndex;
            }
        }

        void shutdown()
        {
            if (m_renderer && m_graph)
            {
                m_renderer->get_frame_graph().remove(m_graph);
            }

            m_atlasCache.clear();
            m_ui.shutdown();
            m_app.shutdown();
        }

    private:
        void build_frame(f32 dt)
        {
            update_frame_stats(dt);

            const auto windowSize = m_app.get_main_window().get_size();
            const vec2u resolution{max(windowSize.x, 1u), max(windowSize.y, 1u)};

            const vec2 size{f32(resolution.x), f32(resolution.y)};
            const auto& events = m_inputQueue.get_events();

            m_ui.begin_frame({events.data(), events.size()}, time::from_seconds(dt), size);

            {
                auto root = panel(m_ui,
                    {rootId},
                    panel_style{
                        .backgroundColor = {0.12f, 0.12f, 0.16f, 1.f},
                        .cornerRadius = 6.f,
                        .padding = {16.f, 16.f, 16.f, 16.f},
                        .direction = layout_direction::top_to_bottom,
                        .gap = 16.f,
                        .width = percent_size(1.f),
                        .height = percent_size(1.f),
                    });

                build_header();
                build_middle();
                build_footer();
            }

            m_ui.end_frame();

            const span drawCommands = m_ui.get_draw_commands();
            const span textures = m_ui.get_textures();
            const span textureCommands = m_ui.get_texture_commands();

            auto& frameGraph = m_renderer->get_frame_graph();

            frameGraph.set_input(m_graph, ui_layout_view::InResolution, resolution).assert_value();
            frameGraph.set_input(m_graph, ui_layout_view::InDrawCommands, drawCommands).assert_value();
            frameGraph.set_input(m_graph, ui_layout_view::InTextures, textures).assert_value();
            frameGraph.set_input(m_graph, ui_layout_view::InTextureCommands, textureCommands).assert_value();
            frameGraph.set_input(m_graph, ui_layout_view::InAtlasCache, &m_atlasCache).assert_value();
        }

        void build_header()
        {
            auto header = panel(m_ui,
                {headerId},
                panel_style{
                    .padding = {8.f, 8.f, 8.f, 8.f},
                    .direction = layout_direction::left_to_right,
                    .gap = 8.f,
                    .width = percent_size(1.f),
                    .height = fit_size(),
                    .alignment = alignment::center_left(),
                });

            string_builder name;

            for (u32 i = 0; i < numHeaderButtons; ++i)
            {
                name.clear().format("Button #{}", i);
                button(m_ui, {firstButtonId + i}, name.as<hashed_string_view>());
            }

            checkbox(m_ui, {showPanelId}, m_showAnimatedPanel, "Show panel"_hsv);
            checkbox(m_ui, {expandPanelId}, m_expandedAnimatedPanel, "Expand"_hsv);
        }

        void build_middle()
        {
            auto middle = panel(m_ui,
                {middleId},
                panel_style{
                    .padding = {8.f, 8.f, 8.f, 8.f},
                    .direction = m_middleDirection == dirHorizontalId ? layout_direction::left_to_right
                                                                      : layout_direction::top_to_bottom,
                    .gap = 16.f,
                    .width = percent_size(1.f),
                    .height = percent_size(0.65f),
                });

            {
                auto sidebar = panel(m_ui,
                    {firstSidebarItemId - 1},
                    panel_style{
                        .direction = layout_direction::top_to_bottom,
                        .gap = 8.f,
                        .width = percent_size(0.25f),
                        .height = percent_size(1.f),
                    });

                string_builder name;

                for (u32 i = 0; i < array_size(m_sidebarChecked); ++i)
                {
                    name.clear().format("Item #{}", i);
                    checkbox(m_ui, {firstSidebarItemId + i}, m_sidebarChecked[i], name.as<hashed_string_view>());
                }

                {
                    auto combo = ui::combo_box_builder{m_ui,
                        {comboEasingId},
                        easing_names[u32(m_selectedEasing)],
                        ui::combo_style{.width = fixed_size(200.f)}};

                    for (u32 i = 0; i < array_size(easing_names); ++i)
                    {
                        if (combo.add_item({comboEasingId + 50u + i}, easing_names[i]))
                        {
                            m_selectedEasing = easing_function(i);
                        }
                    }
                }

                {
                    auto directionGroup = ui::radio_group_builder{m_ui, m_middleDirection};

                    directionGroup.add_option(dirVerticalId, "Vertical");
                    directionGroup.add_option(dirHorizontalId, "Horizontal");
                }

                label(m_ui, {sliderCornerId - 1}, "Corner radius"_hsv);
                slider(m_ui, {sliderCornerId}, m_panelCornerRadius, ui::slider_style{}, 0.f, 40.f);
            }

            if (m_showAnimatedPanel)
            {
                const sizing panelWidth = m_expandedAnimatedPanel ? percent_size(0.6f) : percent_size(0.3f);
                const sizing panelHeight = m_expandedAnimatedPanel ? percent_size(0.9f) : percent_size(0.5f);

                auto animatedPanel = panel(m_ui,
                    {animatedPanelId},
                    panel_style{
                        .backgroundColor = {0.08f, 0.08f, 0.10f, 1.f},
                        .cornerRadius = m_panelCornerRadius,
                        .padding = {16.f, 16.f, 16.f, 16.f},
                        .direction = layout_direction::top_to_bottom,
                        .gap = 8.f,
                        .width = panelWidth,
                        .height = panelHeight,
                        .animation = make_fade_scale_animation(time::from_seconds(1.f), m_selectedEasing),
                    });

                label(m_ui, {animatedLabelId}, m_expandedAnimatedPanel ? "Expanded panel"_hsv : "Animated panel"_hsv);
            }
        }

        void build_footer()
        {
            auto footer = panel(m_ui,
                {footerId},
                panel_style{
                    .padding = {8.f, 8.f, 8.f, 8.f},
                    .direction = layout_direction::left_to_right,
                    .gap = 8.f,
                    .width = percent_size(1.f),
                    .height = fit_size(),
                });

            button(m_ui, {applyId}, "Apply"_hsv);

            const bool close = button(m_ui, {closeId}, "Close"_hsv);

            if (close)
            {
                m_app.get_main_window().destroy();
            }

            const f32 fps = m_avgFrameTime > 0.f ? 1.f / m_avgFrameTime : 0.f;
            m_statusBuilder.clear().format("{} fps | {:.1f} ms", u32(fps + 0.5f), m_avgFrameTime * 1000.f);
            label(m_ui, {statusId}, m_statusBuilder.as<hashed_string_view>());
        }

        void update_frame_stats(f32 dt)
        {
            // Exponential moving average keeps the readout stable instead of jittering
            // frame-to-frame.
            m_avgFrameTime += (dt - m_avgFrameTime) * 0.1f;
        }

    private:
        renderer* m_renderer{};

        frame_graph_registry m_nodeRegistry;
        frame_graph_template m_graphTemplate;

        graphics_app m_app;
        h32<frame_graph_subgraph> m_graph{};

        ui::context m_ui;
        input_queue m_inputQueue;

        ui_atlas_cache m_atlasCache;

        bool m_sidebarChecked[3]{};

        bool m_showAnimatedPanel{};
        bool m_expandedAnimatedPanel{};

        easing_function m_selectedEasing = easing_function::ease_out_bounce;
        layout_id m_middleDirection{radioDirectionId + 1u};
        f32 m_panelCornerRadius{12.f};

        f32 m_avgFrameTime{1.f / 60.f};
        string_builder m_statusBuilder;

        u32 m_frameIndex{};
    };
}

int main()
{
    if (!oblo::platform::init())
    {
        return 1;
    }

    const auto platformShutdown = oblo::finally([] { oblo::platform::shutdown(); });

    oblo::module_manager moduleManager;

    auto& mm = oblo::module_manager::get();

    auto* const logModule = mm.load<oblo::log::log_module>();
    OBLO_ASSERT(logModule);

    logModule->add_sink(oblo::allocate_unique<oblo::log::file_sink>(stderr));

    auto* const vkEngine = mm.load<oblo::vk::vulkan_engine_module>();
    OBLO_ASSERT(vkEngine);

    if (!mm.finalize())
    {
        oblo::log::error("Failed to initialize the UI Layout sandbox engine");
        return 1;
    }

    oblo::ui::ui_layout_sandbox sandbox;

    if (!sandbox.init(*vkEngine))
    {
        return 1;
    }

    sandbox.run();
    sandbox.shutdown();

    return 0;
}
