#include <oblo/ui/layout.hpp>
#include <oblo/ui/layout_impl.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace oblo::ui
{
    namespace
    {
        animated_values make_values(f32 x, f32 y, f32 w, f32 h)
        {
            animated_values v;
            v.boundingBox = {x, y, w, h};
            return v;
        }

        animated_values enter_initial(const animated_values& target, animation_properties)
        {
            animated_values out = target;
            out.boundingBox.width = 0.f;
            out.backgroundColor = {0.f, 0.f, 0.f, 0.f};
            return out;
        }

        animated_values exit_final(const animated_values& initial, animation_properties)
        {
            animated_values out = initial;
            out.backgroundColor.a = 0.f;
            return out;
        }

        animation_config linear_config(time duration)
        {
            animation_config config;
            config.easing = easing_function::linear;
            config.duration = duration;
            config.properties = bounding_box_properties;
            return config;
        }

        const rect* rect_of(layout_state& state, layout_id element)
        {
            const auto* const found = find_element(state, element);
            return found ? &found->effectiveRect : nullptr;
        }
    }

    TEST(ui_easing, bounds)
    {
        for (const auto fn : {
                 easing_function::linear,
                 easing_function::ease_in,
                 easing_function::ease_out,
                 easing_function::ease_in_out,
                 easing_function::ease_in_back,
                 easing_function::ease_out_back,
                 easing_function::ease_out_elastic,
                 easing_function::ease_out_bounce,
             })
        {
            EXPECT_FLOAT_EQ(ease(fn, 0.f), 0.f);
            EXPECT_FLOAT_EQ(ease(fn, 1.f), 1.f);
        }

        const auto middle = ease(easing_function::ease_out, 0.5f);
        EXPECT_GT(middle, 0.f);
        EXPECT_LT(middle, 1.f);

        // cubic ease_out starts fast, so it is above the diagonal
        EXPECT_GE(ease(easing_function::ease_out, 0.5f), 0.5f);
    }

    TEST(ui_animation, new_element_snaps_without_enter_config)
    {
        animation_store store;
        store.begin_frame(time::from_seconds(.5f));

        const auto* result = store.update({1}, {}, {}, make_values(10, 20, 100, 50), linear_config({}));

        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        EXPECT_EQ(store.records()[0].state, animation_state::idle);

        store.end_frame();
    }

    TEST(ui_animation, animates_position_change)
    {
        animation_store store;

        // Frame 1: element settles at x = 10
        store.begin_frame(time::from_seconds(.5f));
        const auto* result = store.update({1}, {}, {}, make_values(10, 20, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        store.end_frame();

        // Frame 2: target moves to x = 30; first frame of the animation still renders the old value
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {}, {}, make_values(30, 20, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, animation_state::animationing);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        store.end_frame();

        // Frame 3: halfway through the animation
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {}, {}, make_values(30, 20, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 20.f);
        store.end_frame();

        // Frame 4: animation completes
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {}, {}, make_values(30, 20, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 30.f);
        EXPECT_EQ(store.records()[0].state, animation_state::idle);
        store.end_frame();
    }

    TEST(ui_animation, animates_background_color)
    {
        animation_store store;

        const auto red = color{1.f, 0.f, 0.f, 1.f};
        const auto blue = color{0.f, 0.f, 1.f, 1.f};

        auto config = linear_config(time::from_seconds(1.f));
        config.properties = animation_property::background_color;

        store.begin_frame(time::from_seconds(.5f));
        auto values = make_values(0, 0, 100, 100);
        values.backgroundColor = red;
        store.update({1}, {}, {}, values, config);
        store.end_frame();

        store.begin_frame(time::from_seconds(.5f));
        values.backgroundColor = blue;
        store.update({1}, {}, {}, values, config);
        store.end_frame();

        store.begin_frame(time::from_seconds(.5f));
        values.backgroundColor = blue;
        const auto* result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->backgroundColor.r, 0.5f);
        EXPECT_FLOAT_EQ(result->backgroundColor.b, 0.5f);
        store.end_frame();
    }

    TEST(ui_animation, no_animation_when_only_parent_moves)
    {
        animation_store store;

        // Frame 1: element at absolute x = 10, parent at x = 0 (relative 10)
        store.begin_frame(time::from_seconds(.5f));
        store.update({1}, {2}, {}, make_values(10, 0, 100, 50), linear_config(time::from_seconds(1.f)));
        store.end_frame();

        // Frame 2: both parent and element move by 10; relative position unchanged, no x animation
        store.begin_frame(time::from_seconds(.5f));
        const auto* result = store.update({1}, {2}, {10, 0}, make_values(20, 0, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, animation_state::idle);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 20.f);
        store.end_frame();

        // Frame 3: element moves within the parent, x animation starts
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {2}, {10, 0}, make_values(25, 0, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, animation_state::animationing);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 20.f);
        store.end_frame();
    }

    TEST(ui_animation, reparenting_animates_position)
    {
        animation_store store;

        store.begin_frame(time::from_seconds(.5f));
        store.update({1}, {2}, {}, make_values(10, 0, 100, 50), linear_config(time::from_seconds(1.f)));
        store.end_frame();

        // The element keeps the same relative offset, but its parent changed and moved.
        // Without reparenting detection this would be attributed to the parent moving.
        store.begin_frame(time::from_seconds(.5f));
        const auto* result = store.update({1}, {3}, {5, 0}, make_values(15, 0, 100, 50), linear_config(time::from_seconds(1.f)));
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, animation_state::animationing);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        store.end_frame();
    }

    TEST(ui_animation, enter_animation)
    {
        animation_store store;

        auto config = linear_config(time::from_seconds(1.f));
        config.properties = bounding_box_properties | animation_property::background_color;
        config.enter.setInitialState = enter_initial;

        // Frame 1: element appears, starts from the enter initial state
        store.begin_frame(time::from_seconds(.5f));
        auto values = make_values(10, 10, 100, 50);
        values.backgroundColor = {1.f, 1.f, 1.f, 1.f};
        const auto* result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, animation_state::entering);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 0.f);
        EXPECT_FLOAT_EQ(result->backgroundColor.a, 0.f);
        store.end_frame();

        // Frame 2: the first advancing frame renders the enter initial state
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 0.f);
        EXPECT_FLOAT_EQ(result->backgroundColor.a, 0.f);
        store.end_frame();

        // Frame 3: halfway towards the target
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 50.f);
        EXPECT_FLOAT_EQ(result->backgroundColor.a, 0.5f);
        store.end_frame();

        // Frame 4: enter animation completes
        store.begin_frame(time::from_seconds(.5f));
        result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, animation_state::idle);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 100.f);
        store.end_frame();
    }

    TEST(ui_animation, exit_animation)
    {
        animation_store store;

        auto config = linear_config(time::from_seconds(1.f));
        config.properties = bounding_box_properties | animation_property::background_color;
        config.exit.setFinalState = exit_final;

        auto values = make_values(10, 10, 100, 50);
        values.backgroundColor = {1.f, 1.f, 1.f, 1.f};

        // Element is declared for one frame
        store.begin_frame(time::from_seconds(.5f));
        store.update({1}, {}, {}, values, config);
        store.end_frame();

        // Element disappears; exit starts from the last rendered state
        store.begin_frame(time::from_seconds(.5f));
        store.end_frame();
        ASSERT_EQ(store.records()[0].state, animation_state::exiting);
        ASSERT_NE(store.try_get({1}), nullptr);
        EXPECT_FLOAT_EQ(store.try_get({1})->backgroundColor.a, 1.f);

        // Exit progresses; the start frame renders the last rendered state
        store.begin_frame(time::from_seconds(.5f));
        store.end_frame();
        EXPECT_FLOAT_EQ(store.try_get({1})->backgroundColor.a, 1.f);

        // Halfway through the exit
        store.begin_frame(time::from_seconds(.5f));
        store.end_frame();
        EXPECT_FLOAT_EQ(store.try_get({1})->backgroundColor.a, 0.5f);

        // Exit completes and the record is removed
        store.begin_frame(time::from_seconds(.5f));
        store.end_frame();
        EXPECT_EQ(store.try_get({1}), nullptr);
        EXPECT_TRUE(store.empty());
    }

    TEST(ui_animation, element_without_exit_config_is_removed)
    {
        animation_store store;

        store.begin_frame(time::from_seconds(.5f));
        store.update({1}, {}, {}, make_values(10, 10, 100, 50), linear_config({}));
        store.end_frame();

        store.begin_frame(time::from_seconds(.5f));
        store.end_frame();

        EXPECT_TRUE(store.empty());
    }

    TEST(ui_animation, zero_duration_snaps)
    {
        animation_store store;

        store.begin_frame(time::from_seconds(.5f));
        store.update({1}, {}, {}, make_values(10, 0, 100, 50), linear_config({}));
        store.end_frame();

        store.begin_frame(time::from_seconds(.5f));
        const auto* result = store.update({1}, {}, {}, make_values(30, 0, 100, 50), linear_config({}));
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 30.f);
        EXPECT_EQ(store.records()[0].state, animation_state::idle);
        store.end_frame();
    }

    TEST(ui_layout, fixed_children_left_to_right)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto a = container_builder{}.width(fixed_size(300)).height(fixed_size(100)).build(*state);
            }
            {
                const auto b = container_builder{}.width(fixed_size(500)).height(fixed_size(100)).build(*state);
            }
        }

        end_frame(*state);

        const auto& elements = state->elements;
        ASSERT_EQ(elements.size(), 3);
        EXPECT_FLOAT_EQ(elements[0].effectiveRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[0].effectiveRect.width, 800.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 300.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.x, 300.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.width, 500.f);

        // Tree structure: root owns both children, in order.
        EXPECT_EQ(elements[0].firstChild, 1);
        EXPECT_EQ(elements[1].nextSibling, 2);
        EXPECT_EQ(elements[1].parentIndex, 0);
        EXPECT_EQ(elements[2].parentIndex, 0);

        destroy_state(state);
    }

    TEST(ui_layout, fixed_children_top_to_bottom)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}
                                  .direction(layout_direction::top_to_bottom)
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .build(*state);
            {
                const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(200)).build(*state);
            }
            {
                const auto b = container_builder{}.width(fixed_size(100)).height(fixed_size(100)).build(*state);
            }
        }

        end_frame(*state);

        const auto& elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 0.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.height, 200.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.y, 200.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.height, 100.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_parent_measures_content)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto panel = container_builder{}.build(*state);
                {
                    const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
                }
                {
                    const auto b = container_builder{}.width(fixed_size(200)).height(fixed_size(30)).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        ASSERT_EQ(elements.size(), 4);

        // The fit panel hugs its content: width = sum of children, height = max child.
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 0.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 300.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.height, 50.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_clamps_to_min_max)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto panel = container_builder{}.width(fit_size(400, 600)).build(*state);
                {
                    const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // Content is 100 wide, but the fit minimum is 400.
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 400.f);

        destroy_state(state);
    }

    TEST(ui_layout, childGap_spaces_children)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time ::from_seconds(0.f));

        {
            const auto root =
                container_builder{}.gap(10.f).width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
            }
            {
                const auto b = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.x, 110.f);

        destroy_state(state);
    }

    TEST(ui_layout, padding_offsets_children)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}
                                  .padding({10, 10, 20, 20})
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .build(*state);
            {
                const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 20.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_child_of_fixed_parent)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto child =
                    container_builder{}.width(percent_size(0.5f)).height(percent_size(0.25f)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 400.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.height, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_child_of_padded_parent_uses_inner_size)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}
                                  .padding({10, 10, 0, 0})
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .build(*state);
            {
                const auto child = container_builder{}.width(percent_size(0.5f)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // 50% of the 780 px inner width, positioned after the left padding.
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 390.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_root_uses_layout_size)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}.width(percent_size(0.5f)).height(percent_size(0.5f)).build(*state);
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[0].effectiveRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[0].effectiveRect.y, 0.f);
        EXPECT_FLOAT_EQ(elements[0].effectiveRect.width, 400.f);
        EXPECT_FLOAT_EQ(elements[0].effectiveRect.height, 300.f);

        destroy_state(state);
    }

    TEST(ui_layout, nested_positions_are_absolute)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}
                                  .padding({10, 10, 20, 20})
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .build(*state);
            {
                const auto inner =
                    container_builder{}.gap(5.f).width(fixed_size(400)).height(fixed_size(200)).build(*state);
                {
                    const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
                }
                {
                    const auto b = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // inner starts after root padding; its children are relative to inner.
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 20.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.y, 20.f);
        EXPECT_FLOAT_EQ(elements[3].effectiveRect.x, 115.f);
        EXPECT_FLOAT_EQ(elements[3].effectiveRect.y, 20.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_child_of_fit_parent)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                // A fit panel measures its content from the fixed child; the percentage
                // child contributes 0 and is expanded against the panel's final size.
                const auto panel = container_builder{}.build(*state);
                {
                    const auto a = container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
                }
                {
                    const auto b = container_builder{}.width(percent_size(0.5f)).height(fixed_size(50)).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 100.f);
        EXPECT_FLOAT_EQ(elements[2].effectiveRect.width, 100.f);
        EXPECT_FLOAT_EQ(elements[3].effectiveRect.width, 50.f);
        EXPECT_FLOAT_EQ(elements[3].effectiveRect.x, 100.f);

        destroy_state(state);
    }

    TEST(ui_layout, layout_feeds_animations_on_layout_change)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(time::from_seconds(1.f));

        // Frame 1: the element settles at 100 px wide.
        begin_frame(*state, time::from_seconds(0.f));
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .animation(config)
                                  .width(fixed_size(100))
                                  .height(fixed_size(50))
                                  .build(*state);
        }
        end_frame(*state);

        const auto* const animated = get_animated(*state, {1});
        ASSERT_NE(animated, nullptr);
        EXPECT_FLOAT_EQ(animated->boundingBox.width, 100.f);

        // Frame 2: the layout resolves a new target; the first frame still renders the old width.
        begin_frame(*state, time::from_seconds(.5f));
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .animation(config)
                                  .width(fixed_size(200))
                                  .height(fixed_size(50))
                                  .build(*state);
        }
        end_frame(*state);

        EXPECT_EQ(state->animations.records()[0].state, animation_state::animationing);
        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 100.f);
        // The tree exposes the effective (currently rendered) rect so parents and
        // siblings follow the animation; the true target lives in the store.
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 100.f);
        EXPECT_FLOAT_EQ(state->animations.records()[0].target.boundingBox.width, 200.f);

        // Frame 3: halfway through the animation.
        begin_frame(*state, time::from_seconds(.5f));
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .animation(config)
                                  .width(fixed_size(200))
                                  .height(fixed_size(50))
                                  .build(*state);
        }
        end_frame(*state);

        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_parent_follows_animated_child)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(time::from_seconds(1.f));

        set_layout_size(*state, {800, 600});

        auto build = [&](f32 childWidth)
        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto panel =
                    container_builder{}.id({1}).animation(config).build(*state);
                {
                    const auto child =
                        container_builder{}.id({2}).animation(config).width(fixed_size(childWidth)).height(
                            fixed_size(50)).build(*state);
                }
            }
        };

        begin_frame(*state, time::from_seconds(0.f));
        build(100.f);
        end_frame(*state);

        ASSERT_EQ(state->elements.size(), 3);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 100.f);

        // Target jumps to 200, first animation frame still renders 100.
        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(get_animated(*state, {2})->boundingBox.width, 100.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {2})->width, 100.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 100.f);

        // Halfway: child and fit parent both at 150.
        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(get_animated(*state, {2})->boundingBox.width, 150.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {2})->width, 150.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_parent_follows_animated_child_top_to_bottom)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(time::from_seconds(1.f));

        set_layout_size(*state, {800, 600});

        // Mirrors the sandbox vertical stack: a fit-height parent holding a fixed
        // block and an animated fixed-height panel. The parent must grow smoothly
        // and push the footer down instead of overflowing it.
        auto build = [&](f32 panelHeight)
        {
            const auto root = container_builder{}
                                  .direction(layout_direction::top_to_bottom)
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .build(*state);
            {
                const auto middle = container_builder{}
                                        .id({1})
                                        .direction(layout_direction::top_to_bottom)
                                        .build(*state);
                {
                    const auto sidebar = container_builder{}
                                             .width(fixed_size(200))
                                             .height(fixed_size(100))
                                             .build(*state);
                }
                {
                    const auto panel = container_builder{}
                                           .id({2})
                                           .animation(config)
                                           .width(fixed_size(200))
                                           .height(fixed_size(panelHeight))
                                           .build(*state);
                }
            }
            {
                const auto footer = container_builder{}
                                        .width(fixed_size(800))
                                        .height(fixed_size(40))
                                        .build(*state);
            }
        };

        begin_frame(*state, time::from_seconds(0.f));
        build(120.f);
        end_frame(*state);

        ASSERT_EQ(state->elements.size(), 5);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->height, 220.f);
        const f32 footerY0 = rect_of(*state, {1})->y + 220.f;

        // Target jumps to 200, first animation frame still renders 120.
        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(rect_of(*state, {2})->height, 120.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->height, 220.f);
        EXPECT_FLOAT_EQ(state->elements[4].effectiveRect.y, footerY0);

        // Halfway: panel at 160, parent at 260, footer pushed by 40.
        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(rect_of(*state, {2})->height, 160.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->height, 260.f);
        EXPECT_FLOAT_EQ(state->elements[4].effectiveRect.y, footerY0 + 40.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_grandparent_follows_animated_leaf)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(time::from_seconds(1.f));

        set_layout_size(*state, {800, 600});

        // Nested fit chain: neither the outer nor the inner parent is animated
        // themselves, so both must pick up the leaf's effective size bottom-up.
        auto build = [&](f32 leafWidth)
        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto outer = container_builder{}.id({1}).build(*state);
                {
                    const auto inner = container_builder{}.build(*state);
                    {
                        const auto leaf = container_builder{}.id({2}).animation(config).width(fixed_size(leafWidth)).height(
                            fixed_size(50)).build(*state);
                    }
                }
            }
        };

        begin_frame(*state, time::from_seconds(0.f));
        build(100.f);
        end_frame(*state);

        ASSERT_EQ(state->elements.size(), 4);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 100.f);

        // Target jumps to 200, first animation frame still renders 100.
        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(get_animated(*state, {2})->boundingBox.width, 100.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {2})->width, 100.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 100.f);

        // Halfway: leaf, inner and outer all at 150.
        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(get_animated(*state, {2})->boundingBox.width, 150.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {2})->width, 150.f);
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, sibling_follows_animated_sibling)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(time::from_seconds(1.f));

        set_layout_size(*state, {800, 600});

        auto build = [&](f32 firstWidth)
        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto a = container_builder{}.id({1}).animation(config).width(fixed_size(firstWidth)).height(
                    fixed_size(50)).build(*state);
            }
            {
                const auto b =
                    container_builder{}.width(fixed_size(100)).height(fixed_size(50)).build(*state);
            }
        };

        begin_frame(*state, time::from_seconds(0.f));
        build(100.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(state->elements[2].effectiveRect.x, 100.f);

        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        // First child still renders 100, sibling stays at 100.
        EXPECT_FLOAT_EQ(state->elements[1].effectiveRect.width, 100.f);
        EXPECT_FLOAT_EQ(state->elements[2].effectiveRect.x, 100.f);

        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        // Halfway: sibling pushed to 150.
        EXPECT_FLOAT_EQ(state->elements[1].effectiveRect.width, 150.f);
        EXPECT_FLOAT_EQ(state->elements[2].effectiveRect.x, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, child_uses_animated_parent_inner_size)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(time::from_seconds(1.f));

        set_layout_size(*state, {800, 600});

        auto build = [&](f32 parentWidth)
        {
            const auto root = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            {
                const auto parent = container_builder{}.id({1}).animation(config).width(fixed_size(parentWidth)).height(
                    fixed_size(100)).build(*state);
                {
                    const auto child = container_builder{}.width(percent_size(1.f)).height(fixed_size(50)).build(*state);
                }
            }
        };

        begin_frame(*state, time::from_seconds(0.f));
        build(100.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(state->elements[2].effectiveRect.width, 100.f);

        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        // Parent still renders 100, percentage child follows the animated inner size.
        EXPECT_FLOAT_EQ(state->elements[1].effectiveRect.width, 100.f);
        EXPECT_FLOAT_EQ(state->elements[2].effectiveRect.width, 100.f);

        begin_frame(*state, time::from_seconds(.5f));
        build(200.f);
        end_frame(*state);

        EXPECT_FLOAT_EQ(state->elements[1].effectiveRect.width, 150.f);
        EXPECT_FLOAT_EQ(state->elements[2].effectiveRect.width, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, id_without_animation_uses_effective_rect)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        begin_frame(*state, time::from_seconds(.5f));
        {
            const auto root = container_builder{}.id({1}).width(fixed_size(100)).height(fixed_size(50)).build(*state);
        }
        end_frame(*state);

        const auto* const found = find_element(*state, {1});
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->animated, nullptr);
        EXPECT_FLOAT_EQ(found->effectiveRect.width, 100.f);

        destroy_state(state);
    }

    TEST(ui_layout, layout_enter_animation)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        auto config = linear_config(time::from_seconds(1.f));
        config.enter.setInitialState = enter_initial;

        begin_frame(*state, time::from_seconds(.5f));
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .animation(config)
                                  .width(fixed_size(100))
                                  .height(fixed_size(50))
                                  .build(*state);
        }
        end_frame(*state);

        EXPECT_EQ(state->animations.records()[0].state, animation_state::entering);
        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 0.f);

        destroy_state(state);
    }

    TEST(ui_layout, layout_exit_animation_keeps_values_while_exiting)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        auto config = linear_config(time::from_seconds(1.f));
        config.exit.setFinalState = exit_final;

        // Frame 1: the element is declared.
        begin_frame(*state, time::from_seconds(.5f));
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .animation(config)
                                  .width(fixed_size(100))
                                  .height(fixed_size(50))
                                  .build(*state);
        }
        end_frame(*state);

        // Frame 2: the element disappears; it keeps animating out.
        begin_frame(*state, time::from_seconds(.5f));
        end_frame(*state);

        EXPECT_TRUE(state->elements.empty());
        EXPECT_EQ(state->animations.records()[0].state, animation_state::exiting);
        ASSERT_NE(get_animated(*state, {1}), nullptr);
        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 100.f);
        EXPECT_EQ(find_element(*state, {1}), nullptr);

        destroy_state(state);
    }

    TEST(ui_layout, align_center_x_within_wider_parent)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .align(alignment::center())
                                  .build(*state);
            {
                const auto child = container_builder{}.width(fixed_size(200)).height(fixed_size(100)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // Single child centered along the main (x) axis of a left-to-right layout.
        // With alignment::center() both axes are centered.
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 300.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 250.f);

        destroy_state(state);
    }

    TEST(ui_layout, align_center_y_within_taller_parent)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            const auto root = container_builder{}
                                  .direction(layout_direction::top_to_bottom)
                                  .width(fixed_size(800))
                                  .height(fixed_size(600))
                                  .align(alignment::center())
                                  .build(*state);
            {
                const auto child = container_builder{}.width(fixed_size(200)).height(fixed_size(100)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // Single child centered along the main (y) axis of a top-to-bottom layout.
        // With alignment::center() both axes are centered.
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 300.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 250.f);

        destroy_state(state);
    }

    TEST(ui_layout, align_centers_percentage_child)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            // A fixed box with a percentage child, both axes centered: the child expands
            // against the box and is centered within it (the checkbox use case).
            const auto root = container_builder{}
                                  .width(fixed_size(100))
                                  .height(fixed_size(100))
                                  .align(alignment::center())
                                  .build(*state);
            {
                const auto child =
                    container_builder{}.width(percent_size(0.5f)).height(percent_size(0.5f)).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.x, 25.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.y, 25.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.width, 50.f);
        EXPECT_FLOAT_EQ(elements[1].effectiveRect.height, 50.f);

        destroy_state(state);
    }

    TEST(ui_layout, hit_test_returns_topmost)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            // bottom: large element with id "bottom"
            const auto bottom = container_builder{}
                                    .id({1})
                                    .width(fixed_size(400))
                                    .height(fixed_size(400))
                                    .build(*state);
            // top: smaller element on top of bottom, overlapping, with id "top"
            const auto top = container_builder{}
                                 .id({2})
                                 .width(fixed_size(100))
                                 .height(fixed_size(100))
                                 .build(*state);
        }

        end_frame(*state);

        // A point inside the top element must report "top", not "bottom".
        EXPECT_EQ(hit_test(*state, {50.f, 50.f}), layout_id{2});
        // A point only inside the bottom element reports "bottom".
        EXPECT_EQ(hit_test(*state, {300.f, 300.f}), layout_id{1});
        // A point outside everything reports an empty id.
        EXPECT_EQ(hit_test(*state, {700.f, 700.f}), layout_id{});

        destroy_state(state);
    }

    TEST(ui_layout, hit_test_ignores_elements_without_id)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, time::from_seconds(0.f));

        {
            // A non-interactive element covering the whole area, no id.
            const auto background = container_builder{}.width(fixed_size(800)).height(fixed_size(600)).build(*state);
            // An interactive element on top.
            const auto top = container_builder{}.id({1}).width(fixed_size(100)).height(fixed_size(100)).build(*state);
        }

        end_frame(*state);

        // The background has no id, so the click passes through to the interactive element.
        EXPECT_EQ(hit_test(*state, {50.f, 50.f}), layout_id{1});

        destroy_state(state);
    }

    TEST(ui_layout, manual_update_element)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        begin_frame(*state, time::from_seconds(.25f));

        const auto* const animated =
            update_element(*state, {1}, {}, {}, make_values(10, 10, 100, 50), linear_config(time::from_seconds(1.f)));

        ASSERT_NE(animated, nullptr);
        EXPECT_EQ(get_animated(*state, {1}), animated);
        end_frame(*state);

        destroy_state(state);
    }
}