#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/ui/forward.hpp>

#include <span>

namespace oblo::ui
{
    // An RGBA color, with channels normalized in the [0, 1] range.
    struct color
    {
        f32 r;
        f32 g;
        f32 b;
        f32 a;
    };

    // An axis aligned rectangle in layout space. x/y are the top-left corner, width/height the size.
    struct rect
    {
        f32 x;
        f32 y;
        f32 width;
        f32 height;

        constexpr vec2 position() const noexcept
        {
            return {x, y};
        }

        constexpr vec2 size() const noexcept
        {
            return {width, height};
        }

        constexpr vec2 max() const noexcept
        {
            return {x + width, y + height};
        }

        constexpr bool contains(const vec2& p) const noexcept
        {
            return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
        }

        static constexpr rect from_extents(const vec2& min, const vec2& max) noexcept
        {
            return {min.x, min.y, max.x - min.x, max.y - min.y};
        }
    };

    // Easing functions take an input in [0, 1] and return an output in the same range,
    // with the exception of back/elastic/bounce curves which can overshoot.
    enum class easing_function : u8
    {
        linear,
        ease_in,
        ease_out,
        ease_in_out,
        ease_in_back,
        ease_out_back,
        ease_out_elastic,
        ease_out_bounce,
    };

    // Which properties of an element participate in a transition.
    // The enum values are bit *indices* for use with the flags<> helper.
    enum class transition_property : u8
    {
        x,
        y,
        width,
        height,
        background_color,
        overlay_color,
        corner_radius,
        enum_max,
    };

    using transition_properties = flags<transition_property>;

    constexpr transition_properties position_properties = transition_property::x | transition_property::y;
    constexpr transition_properties dimensions_properties = transition_property::width | transition_property::height;
    constexpr transition_properties bounding_box_properties = position_properties | dimensions_properties;

    // The values that can be transitioned for a single element. The layout resolves the
    // "target" values each frame, and the transition system interpolates between the
    // previous rendered values and the target over time.
    struct transitioned_values
    {
        rect boundingBox;
        color backgroundColor{};
        color overlayColor{};
        vec4 cornerRadius{};
    };

    // Called when an element first appears. Given the resolved target state, returns the
    // state the element should animate from (e.g. transparent, scaled to zero, offset).
    using enter_state_fn = transitioned_values (*)(const transitioned_values& target, transition_properties properties);

    // Called when an element is removed. Given the state it was last rendered in, returns
    // the state it should animate towards before being discarded.
    using exit_state_fn = transitioned_values (*)(const transitioned_values& initial, transition_properties properties);

    struct transition_enter_config
    {
        enter_state_fn setInitialState{};
        // When true the enter animation also runs if the parent appeared on the same frame.
        // The default skips the enter animation in that case, to avoid animating every
        // element of a freshly created list.
        bool triggerOnFirstParentFrame{false};
    };

    struct transition_exit_config
    {
        exit_state_fn setFinalState{};
    };

    struct transition_config
    {
        f32 duration;
        easing_function easing;
        transition_properties properties;
        transition_enter_config enter{};
        transition_exit_config exit{};
    };

    enum class layout_direction : u8
    {
        left_to_right,
        top_to_bottom,
    };

    enum class sizing_kind : u8
    {
        fit,
        fixed,
        percentage,
    };

    struct fit_sizing
    {
        f32 min;
        f32 max;
    };

    struct fixed_sizing
    {
        f32 size;
    };

    struct percentage_sizing
    {
        f32 size;
    };

    struct sizing
    {
        sizing_kind kind;

        union {
            fit_sizing fit;
            fixed_sizing fixed;
            percentage_sizing percentage;
        } sizing;
    };

    struct padding
    {
        f32 left;
        f32 right;
        f32 top;
        f32 bottom;
    };

    struct container_descriptor
    {
        layout_direction direction;
        sizing width;
        sizing height;

        f32 child_gap;

        vec4 cornerRadius;

        padding padding;

        layout_id elementId;

        bool has_transition;
        transition_config transition;
    };

    // Sentinel index marking the absence of a parent, child or sibling.
    constexpr u32 invalid_index = ~u32{};

    // A single container declared this frame. Elements form a tree; traverse it through
    // parent_index / first_child / next_sibling (indices into the layout state's element tree).
    struct layout_element
    {
        // The descriptor this element was declared with.
        container_descriptor desc{};

        // The stable id, copied from desc for convenience.
        layout_id elementId{};

        // The resolved bounding box for this frame, in absolute layout coordinates. For
        // elements with an active transition this is the target the animation is moving
        // towards; use `animated` for the interpolated box to render.
        rect targetRect{};

        vec4 cornerRadius;

        // The measured content size along the (width, height) axes, before clamping and
        // before any percentage expansion. Only meaningful for fit sizing.
        vec2 contentSize{};

        // Interpolated values to render this frame, or nullptr when the element has no id
        // or no transition configured. When nullptr, target_rect is the final box.
        const transitioned_values* animated{};

        u32 parentIndex{invalid_index};
        u32 firstChild{invalid_index};
        u32 nextSibling{invalid_index};

        // Internal: last child appended, used while building the tree.
        u32 lastChild{invalid_index};
    };

    // The layout state is an opaque object owned by the caller through create_state()/
    // destroy_state(). Its full definition lives in a private header; consumers must go
    // through the free functions below rather than touching its members directly.
    struct layout_state;

    layout_state* create_state();
    void destroy_state(layout_state* state);

    // The available layout area. Percentage sizing on the root is resolved against this.
    void set_layout_size(layout_state& state, vec2 size);

    // Call at the start of every frame, with the frame time in seconds. Clears the
    // previous frame's element tree.
    void begin_frame(layout_state& state, f32 dt);

    // Call at the end of every frame, after all elements have been declared. Resolves the
    // layout, feeds the transition system and advances exiting elements.
    void end_frame(layout_state& state);

    // Returns the first element declared this frame with the given id, or nullptr.
    // Exiting elements are not declared and are not returned here; use get_animated()
    // for those.
    const layout_element* find_element(const layout_state& state, layout_id element);

    // Feeds the resolved target state of an element to the animation system. Used by the
    // layout solver during end_frame; also available for manual use.
    const transitioned_values* update_element(layout_state& state,
        layout_id element,
        layout_id parent,
        vec2 parentOrigin,
        const transitioned_values& target,
        const transition_config& config);

    // Returns the current interpolated values of an element, or nullptr if the element is
    // not being animated.
    const transitioned_values* get_animated(const layout_state& state, layout_id element);

    // All elements declared this frame, in declaration order.
    std::span<const layout_element> get_elements(const layout_state& state);

    void begin_container(layout_state& state, const container_descriptor& desc);
    void end_container(layout_state& state);

    class container_scope
    {
    public:
        container_scope(const container_scope&) = delete;
        container_scope(container_scope&&) noexcept = delete;

        container_scope& operator=(const container_scope&) = delete;
        container_scope& operator=(container_scope&&) noexcept = delete;

        ~container_scope()
        {
            if (m_state)
            {
                end_container(*m_state);
            }
        }

    private:
        container_scope(layout_state& state) : m_state{&state} {}

        layout_state* m_state{};

        friend class container_builder;
    };

    class container_builder
    {
    public:
        container_builder() = default;
        container_builder(const container_builder&) = delete;
        container_builder(container_builder&&) noexcept = delete;

        container_builder& operator=(const container_builder&) = delete;
        container_builder& operator=(container_builder&&) noexcept = delete;

        container_builder&& id(layout_id elementId) &&
        {
            m_desc.elementId = elementId;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& transition(const transition_config& config) &&
        {
            m_desc.transition = config;
            m_desc.has_transition = true;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& direction(layout_direction dir) &&
        {
            m_desc.direction = dir;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& gap(f32 gap) &&
        {
            m_desc.child_gap = gap;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& padding(const padding& p) &&
        {
            m_desc.padding = p;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& width(const fit_sizing& s) &&
        {
            m_desc.width = {
                .kind = sizing_kind::fit,
                .sizing = {.fit = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& width(const fixed_sizing& s) &&
        {
            m_desc.width = {
                .kind = sizing_kind::fixed,
                .sizing = {.fixed = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& width(const percentage_sizing& s) &&
        {
            m_desc.width = {
                .kind = sizing_kind::percentage,
                .sizing = {.percentage = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& height(const fit_sizing& s) &&
        {
            m_desc.height = {
                .kind = sizing_kind::fit,
                .sizing = {.fit = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& height(const fixed_sizing& s) &&
        {
            m_desc.height = {
                .kind = sizing_kind::fixed,
                .sizing = {.fixed = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& height(const percentage_sizing& s) &&
        {
            m_desc.height = {
                .kind = sizing_kind::percentage,
                .sizing = {.percentage = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& corner_radius(f32 r) &&
        {
            m_desc.cornerRadius = vec4::splat(r);
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& corner_radius(vec4 r) &&
        {
            m_desc.cornerRadius = r;
            return static_cast<container_builder&&>(*this);
        }

        container_scope build(layout_state& state) &&
        {
            begin_container(state, m_desc);
            return container_scope{state};
        }

    private:
        container_descriptor m_desc{};
    };
}
