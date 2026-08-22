#include <oblo/ui/game/ui.hpp>

#include <gtest/gtest.h>

#include <cstring>

namespace oblo::ui::game
{
    namespace
    {
        vec2 measure(const char* text, f32 fontHeight)
        {
            return {f32(std::strlen(text)) * fontHeight * 0.5f, fontHeight};
        }

        const rect* rect_of(const context& ctx, layout_id id)
        {
            for (const auto& e : ctx.elements())
            {
                if (e.elementId == id)
                {
                    return &e.targetRect;
                }
            }

            return nullptr;
        }

        input_event ev_move(f32 x, f32 y)
        {
            input_event e{};
            e.kind = input_event_kind::mouse_move;
            e.mouseMove = {x, y};
            return e;
        }

        input_event ev_press()
        {
            input_event e{};
            e.kind = input_event_kind::mouse_press;
            e.mousePress = {mouse_key::left};
            return e;
        }

        input_event ev_release()
        {
            input_event e{};
            e.kind = input_event_kind::mouse_release;
            e.mouseRelease = {mouse_key::left};
            return e;
        }
    }

    TEST(ui_game, emits_draw_intents)
    {
        context ctx;
        ctx.set_measure_text(measure);

        ctx.begin_frame({}, time{}, {800, 600});
        {
            auto panel = begin_panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        EXPECT_EQ(ctx.rects().size(), 2);
        EXPECT_EQ(ctx.texts().size(), 1);
    }

    TEST(ui_game, button_click)
    {
        context ctx;
        ctx.set_measure_text(measure);

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const rect* const b = rect_of(ctx, {2});
        ASSERT_NE(b, nullptr);

        const f32 cx = b->x + b->width * 0.5f;
        const f32 cy = b->y + b->height * 0.5f;

        const input_event frame2[] = {ev_move(cx, cy), ev_press()};
        ctx.begin_frame({frame2, 2}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            EXPECT_FALSE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();

        const input_event frame3[] = {ev_move(cx, cy), ev_release()};
        ctx.begin_frame({frame3, 2}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            EXPECT_TRUE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();
    }

    TEST(ui_game, button_ignores_release_outside)
    {
        context ctx;
        ctx.set_measure_text(measure);

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const rect* const b = rect_of(ctx, {2});
        ASSERT_NE(b, nullptr);

        const f32 cx = b->x + b->width * 0.5f;
        const f32 cy = b->y + b->height * 0.5f;

        const input_event frame2[] = {ev_move(cx, cy), ev_press()};
        ctx.begin_frame({frame2, 2}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const input_event frame3[] = {ev_move(0.f, 0.f), ev_release()};
        ctx.begin_frame({frame3, 2}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            EXPECT_TRUE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();
    }

    TEST(ui_game, slider_drag)
    {
        context ctx;
        ctx.set_measure_text(measure);

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            f32 v = 0.5f;
            slider(ctx, {3}, v, 0.f, 1.f, {});
        }
        ctx.end_frame();

        const rect* const s = rect_of(ctx, {3});
        ASSERT_NE(s, nullptr);

        slider_style style{};
        style.padding = {0.f, 0.f, 0.f, 0.f};
        style.width = 200.f;

        f32 value = 0.5f;

        const input_event frame[] = {ev_move(s->x + 200.f * 0.25f, s->y + s->height * 0.5f), ev_press()};
        ctx.begin_frame({frame, 1}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            EXPECT_TRUE(slider(ctx, {3}, value, 0.f, 1.f, style));
        }
        ctx.end_frame();

        EXPECT_NEAR(value, 0.25f, 0.01f);
    }

    TEST(ui_game, checkbox_toggle)
    {
        context ctx;
        ctx.set_measure_text(measure);

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            bool checked = false;
            checkbox(ctx, {4}, checked, "On", {});
        }
        ctx.end_frame();

        const rect* const c = rect_of(ctx, {4});
        ASSERT_NE(c, nullptr);

        const f32 cx = c->x + c->width * 0.5f;
        const f32 cy = c->y + c->height * 0.5f;

        const input_event frame2[] = {ev_move(cx, cy), ev_press()};
        ctx.begin_frame({frame2, 2}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            bool checked = false;
            checkbox(ctx, {4}, checked, "On", {});
        }
        ctx.end_frame();

        const input_event frame3[] = {ev_move(cx, cy), ev_release()};
        ctx.begin_frame({frame3, 2}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            bool checked = false;
            EXPECT_TRUE(checkbox(ctx, {4}, checked, "On", {}));
            EXPECT_TRUE(checked);
        }
        ctx.end_frame();
    }

    TEST(ui_game, click_within_single_frame)
    {
        context ctx;
        ctx.set_measure_text(measure);

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const rect* const b = rect_of(ctx, {2});
        ASSERT_NE(b, nullptr);

        const f32 cx = b->x + b->width * 0.5f;
        const f32 cy = b->y + b->height * 0.5f;

        // A press and release that both occur within a single frame must still register as a click.
        const input_event frame[] = {ev_move(cx, cy), ev_press(), ev_release()};
        ctx.begin_frame({frame, 3}, time{}, layoutSize);
        {
            auto panel = begin_panel(ctx, {1});
            EXPECT_TRUE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();
    }
}
