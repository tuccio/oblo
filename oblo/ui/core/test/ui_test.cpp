#include <oblo/ui/ui.hpp>

#include <oblo/input/input_event.hpp>

#include <gtest/gtest.h>

#include <cstring>

namespace oblo::ui
{
    namespace
    {
        const rect* rect_of(const context& ctx, layout_id id)
        {
            for (const auto& e : ctx.get_layout_elements())
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

        input_event ev_press(f32 x, f32 y)
        {
            input_event e{};
            e.kind = input_event_kind::mouse_press;
            e.mousePress = {mouse_key::left, x, y};
            return e;
        }

        input_event ev_release(f32 x, f32 y)
        {
            input_event e{};
            e.kind = input_event_kind::mouse_release;
            e.mouseRelease = {mouse_key::left, x, y};
            return e;
        }
    }

    TEST(ui_game, button_click)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const rect* const b = rect_of(ctx, {2});
        ASSERT_NE(b, nullptr);

        const f32 cx = b->x + b->width * 0.5f;
        const f32 cy = b->y + b->height * 0.5f;

        const input_event frame2[] = {ev_move(cx, cy), ev_press(cx, cy)};
        ctx.begin_frame({frame2, 2}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            EXPECT_FALSE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();

        const input_event frame3[] = {ev_move(cx, cy), ev_release(cx, cy)};
        ctx.begin_frame({frame3, 2}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            EXPECT_TRUE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();
    }

    TEST(ui_game, button_ignores_release_outside)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const rect* const b = rect_of(ctx, {2});
        ASSERT_NE(b, nullptr);

        const f32 cx = b->x + b->width * 0.5f;
        const f32 cy = b->y + b->height * 0.5f;

        const input_event frame2[] = {ev_move(cx, cy), ev_press(cx, cy)};
        ctx.begin_frame({frame2, 2}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();

        const input_event frame3[] = {ev_move(0.f, 0.f), ev_release(0.f, 0.f)};
        ctx.begin_frame({frame3, 2}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            EXPECT_TRUE(button(ctx, {2}, "OK"));
        }
        ctx.end_frame();
    }

    TEST(ui_game, checkbox_toggle)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            bool checked = false;
            checkbox(ctx, {4}, checked, "On", {});
        }
        ctx.end_frame();

        const rect* const c = rect_of(ctx, {4});
        ASSERT_NE(c, nullptr);

        const f32 cx = c->x + c->width * 0.5f;
        const f32 cy = c->y + c->height * 0.5f;

        const input_event frame2[] = {ev_move(cx, cy), ev_press(cx, cy)};
        ctx.begin_frame({frame2, 2}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            bool checked = false;
            checkbox(ctx, {4}, checked, "On", {});
        }
        ctx.end_frame();

        const input_event frame3[] = {ev_move(cx, cy), ev_release(cx, cy)};
        ctx.begin_frame({frame3, 2}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            bool checked = false;
            EXPECT_TRUE(checkbox(ctx, {4}, checked, "On", {}));
            EXPECT_TRUE(checked);
        }
        ctx.end_frame();
    }

    TEST(ui_game, click_within_single_frame)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});
            button(ctx, {2}, "OK");
        }
        ctx.end_frame();
    }

    TEST(ui_game, floating_popup_zorder)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {1});

            auto combo = container_builder{}.id({2}).direction(layout_direction::top_to_bottom).build(ctx.get_layout());
            button(ctx, {3}, "Header");

            auto popup = container_builder{}
                             .id({4})
                             .direction(layout_direction::top_to_bottom)
                             .floating(floating_config{.zIndex = 100.f})
                             .build(ctx.get_layout());

            button(ctx, {5}, "Item0");
            button(ctx, {6}, "Item1");
        }
        ctx.end_frame();

        const auto& els = ctx.get_layout_elements();

        i32 idxCombo = -1, idxHeader = -1, idxPopup = -1, idxItem0 = -1, idxItem1 = -1;

        for (i32 i = 0; i < i32(els.size()); ++i)
        {
            const layout_id id = els[i].elementId;

            if (id == layout_id{2})
                idxCombo = i;
            else if (id == layout_id{3})
                idxHeader = i;
            else if (id == layout_id{4})
                idxPopup = i;
            else if (id == layout_id{5})
                idxItem0 = i;
            else if (id == layout_id{6})
                idxItem1 = i;

            if (id == layout_id{4} || id == layout_id{5} || id == layout_id{6})
            {
                EXPECT_GT(els[i].zIndex, 0.f) << "element " << id.value << " zIndex";
            }
        }

        ASSERT_NE(idxCombo, -1);
        ASSERT_NE(idxHeader, -1);
        ASSERT_NE(idxPopup, -1);
        ASSERT_NE(idxItem0, -1);
        ASSERT_NE(idxItem1, -1);

        EXPECT_GT(idxPopup, idxCombo);
        EXPECT_GT(idxItem0, idxPopup);
        EXPECT_GT(idxItem1, idxPopup);
    }

    TEST(ui_game, real_combo_popup_zorder)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        i32 selected = -1;
        const hashed_string_view items[] = {"Apple"_hsv, "Banana"_hsv, "Cherry"_hsv};

        // Establish the open state on frame 1 (the popup-open flag lives on the layout element and
        // is carried across frames), then read it back on frame 2.
        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {100});
            container_builder{}.id({1}).direction(layout_direction::top_to_bottom).build(ctx.get_layout());
            ctx.set_popup_open({1}, true);
        }
        ctx.end_frame();

        ctx.begin_frame({}, time{}, layoutSize);
        {
            auto p = panel(ctx, {100});
            combo_box(ctx, {1}, selected, items, {});
        }
        ctx.end_frame();

        const auto& els = ctx.get_layout_elements();

        i32 idxHeader = -1, idxItem0 = -1, idxItem1 = -1, idxItem2 = -1;
        i32 countFloating = 0;

        for (i32 i = 0; i < i32(els.size()); ++i)
        {
            const layout_id id = els[i].elementId;

            if (id == layout_id{2})
                idxHeader = i; // headerId = id + 1
            else if (id == layout_id{101})
                idxItem0 = i; // id + 100
            else if (id == layout_id{102})
                idxItem1 = i;
            else if (id == layout_id{103})
                idxItem2 = i;

            if (els[i].zIndex > 0.f)
            {
                ++countFloating;
            }
        }

        ASSERT_NE(idxHeader, -1);
        ASSERT_NE(idxItem0, -1);
        ASSERT_NE(idxItem1, -1);
        ASSERT_NE(idxItem2, -1);

        // Entries (items) must be drawn after the header (which is part of the combo panel).
        EXPECT_GT(idxItem0, idxHeader);
        EXPECT_GT(idxItem1, idxHeader);
        EXPECT_GT(idxItem2, idxHeader);
        EXPECT_GT(idxItem1, idxItem0);
        EXPECT_GT(idxItem2, idxItem1);

        // The popup, its three entry buttons, and the three entry labels should all be lifted above
        // the normal flow.
        EXPECT_EQ(countFloating, 7);
    }

    TEST(ui_game, floating_ancestor_does_not_cover_popup)
    {
        context ctx;
        ASSERT_TRUE(ctx.init());

        const vec2 layoutSize{800, 600};

        ctx.begin_frame({}, time{}, layoutSize);
        {
            // A floating ancestor (e.g. a tool window) with a high zIndex.
            auto outer = container_builder{}.id({1}).floating(floating_config{.zIndex = 150.f}).build(ctx.get_layout());

            // Non-floating descendant (the combo box) — must NOT end up above its own popup.
            auto inner = container_builder{}.id({2}).direction(layout_direction::top_to_bottom).build(ctx.get_layout());
            button(ctx, {3}, "Header");

            auto popup = container_builder{}
                             .id({4})
                             .direction(layout_direction::top_to_bottom)
                             .floating(floating_config{.zIndex = 100.f})
                             .build(ctx.get_layout());

            button(ctx, {5}, "Item0");
            button(ctx, {6}, "Item1");
        }
        ctx.end_frame();

        const auto& els = ctx.get_layout_elements();

        i32 idxInner = -1, idxPopup = -1, idxItem0 = -1, idxItem1 = -1;

        for (i32 i = 0; i < i32(els.size()); ++i)
        {
            const layout_id id = els[i].elementId;

            if (id == layout_id{2})
                idxInner = i;
            else if (id == layout_id{4})
                idxPopup = i;
            else if (id == layout_id{5})
                idxItem0 = i;
            else if (id == layout_id{6})
                idxItem1 = i;
        }

        ASSERT_NE(idxInner, -1);
        ASSERT_NE(idxPopup, -1);
        ASSERT_NE(idxItem0, -1);
        ASSERT_NE(idxItem1, -1);

        // The popup (and its entries) must be drawn after the combo box, even though the combo
        // inherited a high zIndex from the floating ancestor.
        EXPECT_GT(idxPopup, idxInner);
        EXPECT_GT(idxItem0, idxInner);
        EXPECT_GT(idxItem1, idxInner);
        EXPECT_GT(idxItem0, idxPopup);
        EXPECT_GT(idxItem1, idxPopup);
    }
}
