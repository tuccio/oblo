#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/ui/forward.hpp>

namespace oblo::ui
{
    struct color
    {
        f32 r;
        f32 g;
        f32 b;
        f32 a;
    };

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

    enum class animation_property : u8
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

    using animation_properties = flags<animation_property>;

    constexpr animation_properties position_properties = animation_property::x | animation_property::y;
    constexpr animation_properties dimensions_properties = animation_property::width | animation_property::height;
    constexpr animation_properties bounding_box_properties = position_properties | dimensions_properties;

    struct animated_values
    {
        rect boundingBox;
        color backgroundColor{};
        color overlayColor{};
        vec4 cornerRadius{};
    };

    // Called when an element first appears. Given the resolved target state, returns the
    // state the element should animate from (e.g. transparent, scaled to zero, offset).
    using enter_state_fn =
        function_ref<animated_values(const animated_values& target, animation_properties properties)>;

    // Called when an element is removed. Given the state it was last rendered in, returns
    // the state it should animate towards before being discarded.
    using exit_state_fn =
        function_ref<animated_values(const animated_values& initial, animation_properties properties)>;

    struct animation_enter_config
    {
        enter_state_fn setInitialState;
        // When true the enter animation also runs if the parent appeared on the same frame.
        // The default skips the enter animation in that case, to avoid animating every
        // element of a freshly created list.
        bool triggerOnFirstParentFrame;
    };

    struct animation_exit_config
    {
        exit_state_fn setFinalState;
    };

    struct animation_config
    {
        time duration;
        easing_function easing;
        animation_properties properties;
        animation_enter_config enter;
        animation_exit_config exit;
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
        layout_id elementId;
        layout_direction direction;

        sizing width;
        sizing height;

        vec4 cornerRadius;

        f32 childGap;
        padding padding;


        animation_config animation;
    };

    constexpr u32 invalid_index = ~u32{};

    struct layout_element
    {
        container_descriptor desc{};

        layout_id elementId{};

        rect targetRect{};

        vec4 cornerRadius;

        // The measured content size along the (width, height) axes, before clamping and
        // before any percentage expansion. Only meaningful for fit sizing.
        vec2 contentSize{};

        // Interpolated values to render this frame, or nullptr when the element has no id
        // or no transition configured. When nullptr, target_rect is the final box.
        const animated_values* animated{};

        u32 parentIndex{invalid_index};
        u32 firstChild{invalid_index};
        u32 nextSibling{invalid_index};
        u32 lastChild{invalid_index};
    };

    struct layout_state;

    layout_state* create_state();
    void destroy_state(layout_state* state);

    void set_layout_size(layout_state& state, vec2 size);

    void begin_frame(layout_state& state, time dt);
    void end_frame(layout_state& state);

    span<const layout_element> get_elements(const layout_state& state);

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

        container_builder&& transition(const animation_config& config) &&
        {
            m_desc.animation = config;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& direction(layout_direction dir) &&
        {
            m_desc.direction = dir;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& gap(f32 gap) &&
        {
            m_desc.childGap = gap;
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
