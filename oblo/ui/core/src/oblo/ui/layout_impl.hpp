#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/ui/layout.hpp>

namespace oblo::ui
{
    enum class transition_state : u8
    {
        idle,
        entering,
        transitioning,
        exiting,
    };

    // Persistent per-element record kept across frames. Elements need a stable id for the
    // store to be able to compare the current target with the previous one.
    struct transition_record
    {
        layout_id elementId{};
        transition_state state{transition_state::idle};

        animated_values initial{};
        animated_values current{};
        animated_values target{};

        animation_properties properties{};
        animation_properties activeProperties{};

        easing_function easing{easing_function::ease_out};
        time duration{};
        time elapsedTime{};

        layout_id parentId{};
        vec2 oldRelativePosition{};
        exit_state_fn exitFinal{};

        bool appearedThisFrame{};
        bool reparented{};
        bool declaredThisFrame{};
        bool transitionOut{};
    };

    f32 ease(easing_function fn, f32 t);

    class transition_store
    {
    public:
        transition_store() = default;
        transition_store(const transition_store&) = delete;
        transition_store(transition_store&&) noexcept = delete;
        transition_store& operator=(const transition_store&) = delete;
        transition_store& operator=(transition_store&&) noexcept = delete;

        void begin_frame(time dt);
        void end_frame();

        // Declares the resolved target state of an element for this frame and advances its
        // animation. parentOrigin is the absolute position of the parent this frame, used to
        // avoid animating an element when only its parent moved.
        // Returns a pointer to the interpolated values to render this frame.
        const animated_values* update(layout_id element,
            layout_id parent,
            vec2 parentOrigin,
            const animated_values& target,
            const animation_config& config);

        // Returns the current interpolated values of an element, or nullptr if the element
        // was never declared or has already finished exiting.
        const animated_values* try_get(layout_id element) const;
        animated_values* try_get(layout_id element);

        // All active records, including elements that are currently exiting.
        std::span<const transition_record> records() const;
        std::span<transition_record> records();

        bool empty() const noexcept;
        usize size() const noexcept;

        void clear() noexcept;

    private:
        transition_record* find_record(layout_id element) noexcept;
        const transition_record* find_record(layout_id element) const noexcept;

        void advance(transition_record& record, time dt);
        void snap_to_target(transition_record& record);
        void start_exit(transition_record& record);

    private:
        dynamic_array<transition_record> m_records;
        time m_dt{};
    };

    struct layout_state
    {
        transition_store animations{};

        dynamic_array<u32> openContainerIdxStack;

        dynamic_array<layout_element> elements;

        vec2 layoutSize{};
    };

    // Returns the first element declared this frame with the given id, or nullptr.
    // Exiting elements are not declared and are not returned here; use get_animated()
    // for those.
    const layout_element* find_element(const layout_state& state, layout_id element);

    // Feeds the resolved target state of an element to the animation system. Used by the
    // layout solver during end_frame; also available for manual use.
    const animated_values* update_element(layout_state& state,
        layout_id element,
        layout_id parent,
        vec2 parentOrigin,
        const animated_values& target,
        const animation_config& config);

    // Returns the current interpolated values of an element, or nullptr if the element is
    // not being animated.
    const animated_values* get_animated(const layout_state& state, layout_id element);
}
