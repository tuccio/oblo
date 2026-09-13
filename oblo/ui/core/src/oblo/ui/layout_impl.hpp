#pragma once

#include <oblo/core/bump_allocator.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flat_hash_map.hpp>
#include <oblo/core/hash.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/ui/font.hpp>
#include <oblo/ui/layout.hpp>

namespace oblo::ui
{
    enum class animation_state : u8
    {
        idle,
        entering,
        animationing,
        exiting,
    };

    // Persistent per-element record kept across frames. Elements need a stable id for the
    // store to be able to compare the current target with the previous one.
    struct animation_record
    {
        layout_id elementId{};
        animation_state state{animation_state::idle};

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
        bool animationOut{};
    };

    f32 ease(easing_function fn, f32 t);

    class animation_store
    {
    public:
        animation_store() = default;
        animation_store(const animation_store&) = delete;
        animation_store(animation_store&&) noexcept = delete;
        animation_store& operator=(const animation_store&) = delete;
        animation_store& operator=(animation_store&&) noexcept = delete;

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

        // Returns the set of properties currently being interpolated for the element.
        // Empty when the element has no record, is idle, or has already snapped to target.
        animation_properties get_active_properties(layout_id element) const noexcept;

        // All active records, including elements that are currently exiting.
        span<const animation_record> records() const;
        span<animation_record> records();

        bool empty() const noexcept;
        usize size() const noexcept;

        void clear() noexcept;

    private:
        animation_record* find_record(layout_id element) noexcept;
        const animation_record* find_record(layout_id element) const noexcept;

        void advance(animation_record& record, time dt);
        void snap_to_target(animation_record& record);
        void start_exit(animation_record& record);

    private:
        dynamic_array<animation_record> m_records;
        time m_dt{};
    };

    struct container_layout_data
    {
        bool isFloating;
        layout_direction direction;
        alignment alignment;

        color backgroundColor;
        vec4 cornerRadius;

        f32 childGap;
        padding padding;

        animation_config animation;

        floating_config floating;
    };

    struct text_layout_data
    {
        hashed_string_view text;
        span<const u32> glyphs;
        color color;
        font_id font;
        u16 fontSize;
    };

    struct layout_element
    {
        layout_element_kind kind{layout_element_kind::container};

        union data {
            container_layout_data container;
            text_layout_data text;

            data() : container{} {};
            data(const data&) = default;
            ~data() = default;
        } data;

        layout_id elementId{};

        sizing width;
        sizing height;

        // The box to render this frame. After pass 1 it holds the true target;
        // after pass 2 (when geometry is animating) it holds the effective
        // (interpolated where active, target otherwise) box. The true target
        // always lives in the animation store record.
        rect effectiveRect{};

        // The measured content size along the (width, height) axes, before clamping and
        // before any percentage expansion. Only meaningful for fit sizing.
        vec2 contentSize{};

        // Interpolated values to render this frame, or nullptr when the element has no id
        // or no animation configured. For geometry axes the effectiveRect already
        // matches the interpolated box; this is still needed to feed the store
        // during layout and for the animated color properties.
        const animated_values* animated{};

        // True when this element (or one of its ancestors) was declared floating. Floating
        // elements are drawn on top of the normal flow and excluded from parent sizing.
        bool isFloating{};

        bool popupOpen{};

        // Resolved draw order for floating elements; higher draws on top.
        f32 zIndex{};

        u32 parentIndex{invalid_index};
        u32 firstChild{invalid_index};
        u32 nextSibling{invalid_index};
        u32 lastChild{invalid_index};

        const rect& get_current_rect() const
        {
            return effectiveRect;
        }

        color get_current_background_color() const
        {
            color result{};

            if (kind == layout_element_kind::container)
            {
                result = animated ? animated->backgroundColor : data.container.backgroundColor;
            }

            return result;
        }

        vec4 get_current_corner_radius() const
        {
            vec4 cornerRadius{};

            if (kind == layout_element_kind::container)
            {
                cornerRadius = animated ? animated->cornerRadius : data.container.cornerRadius;
            }

            return cornerRadius;
        }
    };

    static_assert(std::is_trivially_copyable_v<layout_element>);
    static_assert(std::is_trivially_destructible_v<layout_element>);

    struct layout_state
    {
        bump_allocator frameAllocator{1u << 20};

        animation_store animations{};

        dynamic_array<u32> openContainerIdxStack;

        dynamic_array<layout_element> elements;

        // Resolved elements from the previous frame, used for input hit-testing so that
        // clicks are tested against the geometry the user actually saw last frame. The
        // animated rects are baked in and the animated pointers are cleared to avoid
        // dangling references into the animation store.
        dynamic_array<layout_element> previousElements;

        // Maps an element id to its index in previousElements for O(1) rect lookups.
        flat_hash_map<layout_id, u32> previousElementIndex;

        font_cache fonts;

        vec2 layoutSize{};
    };

    // Returns the first element declared this frame with the given id, or nullptr.
    // Exiting elements are not declared and are not returned here; use get_animated()
    // for those.
    const layout_element* find_element(const layout_state& state, layout_id element);
    layout_element* find_element(layout_state& state, layout_id element);

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
