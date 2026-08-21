#include <oblo/ui/layout.hpp>
#include <oblo/ui/layout_impl.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace oblo::ui
{
    namespace
    {
        transitioned_values make_values(f32 x, f32 y, f32 w, f32 h)
        {
            transitioned_values v;
            v.boundingBox = {x, y, w, h};
            return v;
        }

        transitioned_values enter_initial(const transitioned_values& target, transition_properties)
        {
            transitioned_values out = target;
            out.boundingBox.width = 0.f;
            out.backgroundColor = {0.f, 0.f, 0.f, 0.f};
            return out;
        }

        transitioned_values exit_final(const transitioned_values& initial, transition_properties)
        {
            transitioned_values out = initial;
            out.backgroundColor.a = 0.f;
            return out;
        }

        transition_config linear_config(f32 duration = 1.f)
        {
            transition_config config;
            config.easing = easing_function::linear;
            config.duration = duration;
            config.properties = bounding_box_properties;
            return config;
        }

        const rect* rect_of(layout_state& state, layout_id element)
        {
            const auto* const found = find_element(state, element);
            return found ? &found->targetRect : nullptr;
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

    TEST(ui_transition, new_element_snaps_without_enter_config)
    {
        transition_store store;
        store.begin_frame(0.5f);

        const auto* result = store.update({1}, {}, {}, make_values(10, 20, 100, 50), linear_config());

        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        EXPECT_EQ(store.records()[0].state, transition_state::idle);

        store.end_frame();
    }

    TEST(ui_transition, animates_position_change)
    {
        transition_store store;

        // Frame 1: element settles at x = 10
        store.begin_frame(0.5f);
        const auto* result = store.update({1}, {}, {}, make_values(10, 20, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        store.end_frame();

        // Frame 2: target moves to x = 30; first frame of the transition still renders the old value
        store.begin_frame(0.5f);
        result = store.update({1}, {}, {}, make_values(30, 20, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, transition_state::transitioning);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        store.end_frame();

        // Frame 3: halfway through the transition
        store.begin_frame(0.5f);
        result = store.update({1}, {}, {}, make_values(30, 20, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 20.f);
        store.end_frame();

        // Frame 4: transition completes
        store.begin_frame(0.5f);
        result = store.update({1}, {}, {}, make_values(30, 20, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 30.f);
        EXPECT_EQ(store.records()[0].state, transition_state::idle);
        store.end_frame();
    }

    TEST(ui_transition, animates_background_color)
    {
        transition_store store;

        const auto red = color{1.f, 0.f, 0.f, 1.f};
        const auto blue = color{0.f, 0.f, 1.f, 1.f};

        auto config = linear_config();
        config.properties = transition_property::background_color;

        store.begin_frame(0.5f);
        auto values = make_values(0, 0, 100, 100);
        values.backgroundColor = red;
        store.update({1}, {}, {}, values, config);
        store.end_frame();

        store.begin_frame(0.5f);
        values.backgroundColor = blue;
        store.update({1}, {}, {}, values, config);
        store.end_frame();

        store.begin_frame(0.5f);
        values.backgroundColor = blue;
        const auto* result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->backgroundColor.r, 0.5f);
        EXPECT_FLOAT_EQ(result->backgroundColor.b, 0.5f);
        store.end_frame();
    }

    TEST(ui_transition, no_animation_when_only_parent_moves)
    {
        transition_store store;

        // Frame 1: element at absolute x = 10, parent at x = 0 (relative 10)
        store.begin_frame(0.5f);
        store.update({1}, {2}, {}, make_values(10, 0, 100, 50), linear_config());
        store.end_frame();

        // Frame 2: both parent and element move by 10; relative position unchanged, no x animation
        store.begin_frame(0.5f);
        const auto* result = store.update({1}, {2}, {10, 0}, make_values(20, 0, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, transition_state::idle);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 20.f);
        store.end_frame();

        // Frame 3: element moves within the parent, x animation starts
        store.begin_frame(0.5f);
        result = store.update({1}, {2}, {10, 0}, make_values(25, 0, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, transition_state::transitioning);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 20.f);
        store.end_frame();
    }

    TEST(ui_transition, reparenting_animates_position)
    {
        transition_store store;

        store.begin_frame(0.5f);
        store.update({1}, {2}, {}, make_values(10, 0, 100, 50), linear_config());
        store.end_frame();

        // The element keeps the same relative offset, but its parent changed and moved.
        // Without reparenting detection this would be attributed to the parent moving.
        store.begin_frame(0.5f);
        const auto* result = store.update({1}, {3}, {5, 0}, make_values(15, 0, 100, 50), linear_config());
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, transition_state::transitioning);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 10.f);
        store.end_frame();
    }

    TEST(ui_transition, enter_animation)
    {
        transition_store store;

        auto config = linear_config();
        config.properties = bounding_box_properties | transition_property::background_color;
        config.enter.setInitialState = enter_initial;

        // Frame 1: element appears, starts from the enter initial state
        store.begin_frame(0.5f);
        auto values = make_values(10, 10, 100, 50);
        values.backgroundColor = {1.f, 1.f, 1.f, 1.f};
        const auto* result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, transition_state::entering);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 0.f);
        EXPECT_FLOAT_EQ(result->backgroundColor.a, 0.f);
        store.end_frame();

        // Frame 2: the first advancing frame renders the enter initial state
        store.begin_frame(0.5f);
        result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 0.f);
        EXPECT_FLOAT_EQ(result->backgroundColor.a, 0.f);
        store.end_frame();

        // Frame 3: halfway towards the target
        store.begin_frame(0.5f);
        result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 50.f);
        EXPECT_FLOAT_EQ(result->backgroundColor.a, 0.5f);
        store.end_frame();

        // Frame 4: enter animation completes
        store.begin_frame(0.5f);
        result = store.update({1}, {}, {}, values, config);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(store.records()[0].state, transition_state::idle);
        EXPECT_FLOAT_EQ(result->boundingBox.width, 100.f);
        store.end_frame();
    }

    TEST(ui_transition, exit_animation)
    {
        transition_store store;

        auto config = linear_config();
        config.properties = bounding_box_properties | transition_property::background_color;
        config.exit.setFinalState = exit_final;

        auto values = make_values(10, 10, 100, 50);
        values.backgroundColor = {1.f, 1.f, 1.f, 1.f};

        // Element is declared for one frame
        store.begin_frame(0.5f);
        store.update({1}, {}, {}, values, config);
        store.end_frame();

        // Element disappears; exit starts from the last rendered state
        store.begin_frame(0.5f);
        store.end_frame();
        ASSERT_EQ(store.records()[0].state, transition_state::exiting);
        ASSERT_NE(store.try_get({1}), nullptr);
        EXPECT_FLOAT_EQ(store.try_get({1})->backgroundColor.a, 1.f);

        // Exit progresses; the start frame renders the last rendered state
        store.begin_frame(0.5f);
        store.end_frame();
        EXPECT_FLOAT_EQ(store.try_get({1})->backgroundColor.a, 1.f);

        // Halfway through the exit
        store.begin_frame(0.5f);
        store.end_frame();
        EXPECT_FLOAT_EQ(store.try_get({1})->backgroundColor.a, 0.5f);

        // Exit completes and the record is removed
        store.begin_frame(0.5f);
        store.end_frame();
        EXPECT_EQ(store.try_get({1}), nullptr);
        EXPECT_TRUE(store.empty());
    }

    TEST(ui_transition, element_without_exit_config_is_removed)
    {
        transition_store store;

        store.begin_frame(0.5f);
        store.update({1}, {}, {}, make_values(10, 10, 100, 50), linear_config());
        store.end_frame();

        store.begin_frame(0.5f);
        store.end_frame();

        EXPECT_TRUE(store.empty());
    }

    TEST(ui_transition, zero_duration_snaps)
    {
        transition_store store;

        store.begin_frame(0.5f);
        store.update({1}, {}, {}, make_values(10, 0, 100, 50), linear_config(0.f));
        store.end_frame();

        store.begin_frame(0.5f);
        const auto* result = store.update({1}, {}, {}, make_values(30, 0, 100, 50), linear_config(0.f));
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->boundingBox.x, 30.f);
        EXPECT_EQ(store.records()[0].state, transition_state::idle);
        store.end_frame();
    }

    TEST(ui_layout, fixed_children_left_to_right)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}.width(fixed_sizing{800}).height(fixed_sizing{600}).build(*state);
            {
                const auto a = container_builder{}.width(fixed_sizing{300}).height(fixed_sizing{100}).build(*state);
            }
            {
                const auto b = container_builder{}.width(fixed_sizing{500}).height(fixed_sizing{100}).build(*state);
            }
        }

        end_frame(*state);

        const auto& elements = state->elements;
        ASSERT_EQ(elements.size(), 3);
        EXPECT_FLOAT_EQ(elements[0].targetRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[0].targetRect.width, 800.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.width, 300.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.x, 300.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.width, 500.f);

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
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}
                                  .direction(layout_direction::top_to_bottom)
                                  .width(fixed_sizing{800})
                                  .height(fixed_sizing{600})
                                  .build(*state);
            {
                const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{200}).build(*state);
            }
            {
                const auto b = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{100}).build(*state);
            }
        }

        end_frame(*state);

        const auto& elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].targetRect.y, 0.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.height, 200.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.y, 200.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.height, 100.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_parent_measures_content)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}.width(fixed_sizing{800}).height(fixed_sizing{600}).build(*state);
            {
                const auto panel = container_builder{}.build(*state);
                {
                    const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
                }
                {
                    const auto b = container_builder{}.width(fixed_sizing{200}).height(fixed_sizing{30}).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        ASSERT_EQ(elements.size(), 4);

        // The fit panel hugs its content: width = sum of children, height = max child.
        EXPECT_FLOAT_EQ(elements[1].targetRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.y, 0.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.width, 300.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.height, 50.f);

        destroy_state(state);
    }

    TEST(ui_layout, fit_clamps_to_min_max)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}.width(fixed_sizing{800}).height(fixed_sizing{600}).build(*state);
            {
                const auto panel = container_builder{}.width(fit_sizing{400, 600}).build(*state);
                {
                    const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // Content is 100 wide, but the fit minimum is 400.
        EXPECT_FLOAT_EQ(elements[1].targetRect.width, 400.f);

        destroy_state(state);
    }

    TEST(ui_layout, child_gap_spaces_children)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root =
                container_builder{}.gap(10.f).width(fixed_sizing{800}).height(fixed_sizing{600}).build(*state);
            {
                const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
            }
            {
                const auto b = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].targetRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.x, 110.f);

        destroy_state(state);
    }

    TEST(ui_layout, padding_offsets_children)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}
                                  .padding({10, 10, 20, 20})
                                  .width(fixed_sizing{800})
                                  .height(fixed_sizing{600})
                                  .build(*state);
            {
                const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].targetRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.y, 20.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_child_of_fixed_parent)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}.width(fixed_sizing{800}).height(fixed_sizing{600}).build(*state);
            {
                const auto child =
                    container_builder{}.width(percentage_sizing{0.5f}).height(percentage_sizing{0.25f}).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].targetRect.width, 400.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.height, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_child_of_padded_parent_uses_inner_size)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}
                                  .padding({10, 10, 0, 0})
                                  .width(fixed_sizing{800})
                                  .height(fixed_sizing{600})
                                  .build(*state);
            {
                const auto child = container_builder{}.width(percentage_sizing{0.5f}).build(*state);
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // 50% of the 780 px inner width, positioned after the left padding.
        EXPECT_FLOAT_EQ(elements[1].targetRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.width, 390.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_root_uses_layout_size)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root =
                container_builder{}.width(percentage_sizing{0.5f}).height(percentage_sizing{0.5f}).build(*state);
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[0].targetRect.x, 0.f);
        EXPECT_FLOAT_EQ(elements[0].targetRect.y, 0.f);
        EXPECT_FLOAT_EQ(elements[0].targetRect.width, 400.f);
        EXPECT_FLOAT_EQ(elements[0].targetRect.height, 300.f);

        destroy_state(state);
    }

    TEST(ui_layout, nested_positions_are_absolute)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}
                                  .padding({10, 10, 20, 20})
                                  .width(fixed_sizing{800})
                                  .height(fixed_sizing{600})
                                  .build(*state);
            {
                const auto inner =
                    container_builder{}.gap(5.f).width(fixed_sizing{400}).height(fixed_sizing{200}).build(*state);
                {
                    const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
                }
                {
                    const auto b = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        // inner starts after root padding; its children are relative to inner.
        EXPECT_FLOAT_EQ(elements[1].targetRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[1].targetRect.y, 20.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.x, 10.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.y, 20.f);
        EXPECT_FLOAT_EQ(elements[3].targetRect.x, 115.f);
        EXPECT_FLOAT_EQ(elements[3].targetRect.y, 20.f);

        destroy_state(state);
    }

    TEST(ui_layout, percentage_child_of_fit_parent)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        set_layout_size(*state, {800, 600});
        begin_frame(*state, 0.f);

        {
            const auto root = container_builder{}.width(fixed_sizing{800}).height(fixed_sizing{600}).build(*state);
            {
                // A fit panel measures its content from the fixed child; the percentage
                // child contributes 0 and is expanded against the panel's final size.
                const auto panel = container_builder{}.build(*state);
                {
                    const auto a = container_builder{}.width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
                }
                {
                    const auto b =
                        container_builder{}.width(percentage_sizing{0.5f}).height(fixed_sizing{50}).build(*state);
                }
            }
        }

        end_frame(*state);

        const std::span elements = state->elements;
        EXPECT_FLOAT_EQ(elements[1].targetRect.width, 100.f);
        EXPECT_FLOAT_EQ(elements[2].targetRect.width, 100.f);
        EXPECT_FLOAT_EQ(elements[3].targetRect.width, 50.f);
        EXPECT_FLOAT_EQ(elements[3].targetRect.x, 100.f);

        destroy_state(state);
    }

    TEST(ui_layout, layout_feeds_transitions_on_layout_change)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        const auto config = linear_config(1.f);

        // Frame 1: the element settles at 100 px wide.
        begin_frame(*state, 0.5f);
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .transition(config)
                                  .width(fixed_sizing{100})
                                  .height(fixed_sizing{50})
                                  .build(*state);
        }
        end_frame(*state);

        const auto* const animated = get_animated(*state, {1});
        ASSERT_NE(animated, nullptr);
        EXPECT_FLOAT_EQ(animated->boundingBox.width, 100.f);

        // Frame 2: the layout resolves a new target; the first frame still renders the old width.
        begin_frame(*state, 0.5f);
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .transition(config)
                                  .width(fixed_sizing{200})
                                  .height(fixed_sizing{50})
                                  .build(*state);
        }
        end_frame(*state);

        EXPECT_EQ(state->animations.records()[0].state, transition_state::transitioning);
        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 100.f);
        // The tree exposes the target, the store the interpolated values.
        EXPECT_FLOAT_EQ(rect_of(*state, {1})->width, 200.f);

        // Frame 3: halfway through the transition.
        begin_frame(*state, 0.5f);
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .transition(config)
                                  .width(fixed_sizing{200})
                                  .height(fixed_sizing{50})
                                  .build(*state);
        }
        end_frame(*state);

        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 150.f);

        destroy_state(state);
    }

    TEST(ui_layout, id_without_transition_uses_target_rect)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        begin_frame(*state, 0.f);
        {
            const auto root =
                container_builder{}.id({1}).width(fixed_sizing{100}).height(fixed_sizing{50}).build(*state);
        }
        end_frame(*state);

        const auto* const found = find_element(*state, {1});
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->animated, nullptr);
        EXPECT_FLOAT_EQ(found->targetRect.width, 100.f);

        destroy_state(state);
    }

    TEST(ui_layout, layout_enter_animation)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        auto config = linear_config(1.f);
        config.enter.setInitialState = enter_initial;

        begin_frame(*state, 0.5f);
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .transition(config)
                                  .width(fixed_sizing{100})
                                  .height(fixed_sizing{50})
                                  .build(*state);
        }
        end_frame(*state);

        EXPECT_EQ(state->animations.records()[0].state, transition_state::entering);
        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 0.f);

        destroy_state(state);
    }

    TEST(ui_layout, layout_exit_animation_keeps_values_while_exiting)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        auto config = linear_config(1.f);
        config.exit.setFinalState = exit_final;

        // Frame 1: the element is declared.
        begin_frame(*state, 0.5f);
        {
            const auto root = container_builder{}
                                  .id({1})
                                  .transition(config)
                                  .width(fixed_sizing{100})
                                  .height(fixed_sizing{50})
                                  .build(*state);
        }
        end_frame(*state);

        // Frame 2: the element disappears; it keeps animating out.
        begin_frame(*state, 0.5f);
        end_frame(*state);

        EXPECT_TRUE(state->elements.empty());
        EXPECT_EQ(state->animations.records()[0].state, transition_state::exiting);
        ASSERT_NE(get_animated(*state, {1}), nullptr);
        EXPECT_FLOAT_EQ(get_animated(*state, {1})->boundingBox.width, 100.f);
        EXPECT_EQ(find_element(*state, {1}), nullptr);

        destroy_state(state);
    }

    TEST(ui_layout, manual_update_element)
    {
        auto* const state = create_state();
        ASSERT_NE(state, nullptr);

        begin_frame(*state, 0.25f);
        const auto* const animated = update_element(*state, {1}, {}, {}, make_values(10, 10, 100, 50), linear_config());
        ASSERT_NE(animated, nullptr);
        EXPECT_EQ(get_animated(*state, {1}), animated);
        end_frame(*state);

        destroy_state(state);
    }
}