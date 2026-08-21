#include <ui_layout_sandbox_layout.hpp>

#include <ui_layout_render_node.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/ui/layout.hpp>

#include <cmath>
#include <iterator>

namespace oblo::sandbox
{
    namespace
    {
        ui::layout_state* g_layoutState{};
        bool g_initialized{};
        f32 g_time{};

        ui::color palette_for_id(ui::layout_id id)
        {
            constexpr ui::color palette[] = {
                {.r = 0.14f, .g = 0.14f, .b = 0.18f, .a = 1.f},
                {.r = 0.22f, .g = 0.55f, .b = 0.88f, .a = 1.f},
                {.r = 0.35f, .g = 0.78f, .b = 0.45f, .a = 1.f},
                {.r = 0.93f, .g = 0.62f, .b = 0.25f, .a = 1.f},
                {.r = 0.78f, .g = 0.35f, .b = 0.55f, .a = 1.f},
                {.r = 0.45f, .g = 0.45f, .b = 0.85f, .a = 1.f},
                {.r = 0.85f, .g = 0.75f, .b = 0.35f, .a = 1.f},
                {.r = 0.20f, .g = 0.75f, .b = 0.78f, .a = 1.f},
                {.r = 0.60f, .g = 0.35f, .b = 0.85f, .a = 1.f},
                {.r = 0.95f, .g = 0.35f, .b = 0.30f, .a = 1.f},
                {.r = 0.95f, .g = 0.45f, .b = 0.20f, .a = 1.f},
                {.r = 0.90f, .g = 0.25f, .b = 0.70f, .a = 1.f},
                {.r = 0.25f, .g = 0.85f, .b = 0.55f, .a = 1.f},
                {.r = 0.45f, .g = 0.70f, .b = 0.95f, .a = 1.f},
                {.r = 0.65f, .g = 0.50f, .b = 0.90f, .a = 1.f},
                {.r = 0.95f, .g = 0.80f, .b = 0.45f, .a = 1.f},
                {.r = 0.70f, .g = 0.85f, .b = 0.30f, .a = 1.f},
                {.r = 0.30f, .g = 0.65f, .b = 0.40f, .a = 1.f},
                {.r = 0.75f, .g = 0.30f, .b = 0.30f, .a = 1.f},
                {.r = 0.30f, .g = 0.40f, .b = 0.75f, .a = 1.f},
                {.r = 0.85f, .g = 0.50f, .b = 0.65f, .a = 1.f},
                {.r = 0.40f, .g = 0.80f, .b = 0.75f, .a = 1.f},
            };

            return palette[usize(id.value) % array_size(palette)];
        }

        ui::transition_config default_transition()
        {
            return {
                .duration = 0.25f,
                .easing = ui::easing_function::ease_out,
                .properties = ui::bounding_box_properties,
            };
        }
    }

    void ui_layout_init()
    {
        if (!g_initialized)
        {
            g_layoutState = ui::create_state();
            ui::set_layout_size(*g_layoutState, {1280.f, 720.f});
            g_time = 0.f;
            g_initialized = true;
        }
    }

    void ui_layout_set_size(vec2 size)
    {
        if (g_layoutState)
        {
            ui::set_layout_size(*g_layoutState, size);
        }
    }

    void ui_layout_update(f32 dt)
    {
        if (!g_layoutState)
        {
            ui_layout_init();
        }

        ui::layout_state& state = *g_layoutState;

        g_time += dt;

        ui::begin_frame(state, dt);

        // A slowly oscillating width for the sidebar, so the transition system is exercised.
        const f32 animatedWidth = 0.2f + 0.15f * (0.5f + 0.5f * std::sin(g_time * 1.5f));

        {
            auto root = ui::container_builder{}
                            .id({1})
                            .direction(ui::layout_direction::top_to_bottom)
                            .gap(16.f)
                            .padding({.left = 16.f, .right = 16.f, .top = 16.f, .bottom = 16.f})
                            .width(ui::percentage_sizing{1.f})
                            .height(ui::percentage_sizing{1.f})
                            .build(state);

            {
                auto header = ui::container_builder{}
                                  .id({2})
                                  .direction(ui::layout_direction::left_to_right)
                                  .gap(8.f)
                                  .height(ui::fit_sizing{})
                                  .build(state);

                for (u32 i = 0; i < 4; ++i)
                {
                    ui::container_builder{}
                        .id({100 + i})
                        .width(ui::fixed_sizing{72.f + f32(i) * 24.f})
                        .height(ui::fixed_sizing{32.f})
                        .corner_radius(8.f)
                        .transition(default_transition())
                        .build(state);
                }
            }

            {
                auto middle = ui::container_builder{}
                                  .id({3})
                                  .direction(ui::layout_direction::left_to_right)
                                  .gap(16.f)
                                  .height(ui::percentage_sizing{0.65f})
                                  .build(state);

                {
                    auto sidebar = ui::container_builder{}
                                       .id({4})
                                       .direction(ui::layout_direction::top_to_bottom)
                                       .gap(8.f)
                                       .width(ui::percentage_sizing{animatedWidth})
                                       .transition(default_transition())
                                       .build(state);

                    for (u32 i = 0; i < 6; ++i)
                    {
                        ui::container_builder{}
                            .id({200 + i})
                            .height(ui::fixed_sizing{36.f})
                            .transition(default_transition())
                            .build(state);
                    }
                }

                {
                    auto content = ui::container_builder{}
                                       .id({5})
                                       .direction(ui::layout_direction::left_to_right)
                                       .gap(8.f)
                                       .build(state);

                    for (u32 i = 0; i < 8; ++i)
                    {
                        ui::container_builder{}
                            .id({300 + i})
                            .width(ui::fixed_sizing{48.f + f32(i % 3) * 32.f})
                            .transition(default_transition())
                            .build(state);
                    }
                }
            }

            {
                ui::container_builder{}
                    .id({6})
                    .height(ui::fixed_sizing{32.f})
                    .transition(default_transition())
                    .build(state);
            }
        }

        ui::end_frame(state);
    }

    void get_ui_layout_elements(dynamic_array<ui_layout_element_gpu>& out)
    {
        if (!g_layoutState)
        {
            out.clear();
            return;
        }

        const std::span elements = ui::get_elements(*g_layoutState);

        out.assign_default(elements.size());

        for (usize i = 0; i < elements.size(); ++i)
        {
            const auto& e = elements[i];

            const ui::rect rect = e.animated ? e.animated->boundingBox : e.targetRect;
            const ui::color color = palette_for_id(e.elementId);

            out[i] = {
                .rect = {rect.x, rect.y, rect.width, rect.height},
                .color = {color.r, color.g, color.b, color.a},
                .cornerRadius = e.animated ? e.animated->cornerRadius : e.cornerRadius,
            };
        }
    }
}