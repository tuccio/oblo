#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/flags.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/types.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/ui/forward.hpp>

namespace oblo::ui
{
    struct font;
    using font_id = h16<font>;

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
        rect boundingBox{};
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

    enum class layout_element_kind : u8
    {
        container,
        text,
    };

    enum class alignment_x : u8
    {
        left,
        center,
        right,
    };

    enum class alignment_y : u8
    {
        top,
        center,
        bottom,
    };

    struct alignment
    {
        alignment_x x;
        alignment_y y;

        static constexpr alignment center_left() noexcept
        {
            return {alignment_x::left, alignment_y::center};
        }

        static constexpr alignment top_left() noexcept
        {
            return {alignment_x::left, alignment_y::top};
        }

        static constexpr alignment center() noexcept
        {
            return {alignment_x::center, alignment_y::center};
        }

        static constexpr alignment top_right() noexcept
        {
            return {alignment_x::right, alignment_y::top};
        }

        static constexpr alignment bottom_left() noexcept
        {
            return {alignment_x::left, alignment_y::bottom};
        }

        static constexpr alignment bottom_right() noexcept
        {
            return {alignment_x::right, alignment_y::bottom};
        }
    };

    // Describes how a "floating" element is positioned. A floating element is laid out like a
    // normal child for sizing, but it does not occupy space in its parent's flow and is instead
    // placed at an absolute position derived from an anchor. It (and its whole subtree) is drawn
    // and hit-tested on top of non-floating elements according to its zIndex.
    struct floating_config
    {
        // Element to anchor against. Empty means the floating element's own parent.
        layout_id anchorId{};

        // Which corner of the anchor this element attaches to.
        alignment anchorPoint{alignment::top_left()};

        // Which corner of this element aligns with the anchor point.
        alignment selfPoint{alignment::top_left()};

        // Extra pixel offset applied after anchoring.
        vec2 offset{};

        // Higher values draw on top; floating elements are always drawn after non-floating ones.
        f32 zIndex{0.f};

        // Clamp the floating element to stay within its anchor's bounds.
        bool clipToAnchor{false};
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

    struct percent_sizing
    {
        f32 size;
    };

    struct sizing
    {
        sizing_kind kind;

        union {
            fit_sizing fit;
            fixed_sizing fixed;
            percent_sizing percentage;
        } sizing;
    };

    constexpr sizing fit_size(f32 min = 0.f, f32 max = 0.f) noexcept
    {
        return {sizing_kind::fit, {.fit = {min, max}}};
    }

    constexpr sizing fixed_size(f32 s) noexcept
    {
        return {sizing_kind::fixed, {.fixed = {s}}};
    }

    constexpr sizing percent_size(f32 s) noexcept
    {
        return {sizing_kind::percentage, {.percentage = {s}}};
    }

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

        color backgroundColor;
        vec4 cornerRadius;

        f32 childGap;
        padding padding;

        alignment alignment{alignment::top_left()};

        animation_config animation;

        floating_config floating{};
        bool isFloating{};
    };

    struct text_descriptor
    {
        layout_id elementId;

        hashed_string_view text;

        color color;

        font_id font;
        u16 fontSize;
    };

    constexpr u32 invalid_index = ~u32{};

    struct container_layout_data
    {
        layout_direction direction;
        alignment alignment;

        color backgroundColor;
        vec4 cornerRadius;

        f32 childGap;
        padding padding;

        animation_config animation;

        floating_config floating;
        bool isFloating{};
    };

    struct text_layout_data
    {
        hashed_string_view text;
        std::span<const u32> glyphs;
        color color;
        font_id font;
        u16 fontSize;
    };

    struct layout_element;
    struct layout_state;

    layout_state* create_state();
    void destroy_state(layout_state* state);

    expected<font_id> load_font_from_file(layout_state& state, cstring_view path);
    expected<font_id> load_font_from_memory(layout_state& state, span<const byte> data);

    void set_layout_size(layout_state& state, vec2 size);

    void begin_frame(layout_state& state, time dt);
    void end_frame(layout_state& state);

    // Returns the id of the topmost previous-frame element containing the point, or an
    // empty id if none. Elements without an id are ignored, so non-interactive geometry
    // does not capture input. Reverse iteration gives draw order (later = on top).
    layout_id hit_test(const layout_state& state, vec2 point);

    // Returns the previous-frame rect of the element with the given id, or nullptr.
    const rect* get_rect(const layout_state& state, layout_id id);

    bool is_popup_open(const layout_state& state, layout_id id);
    void set_popup_open(layout_state& state, layout_id id, bool open);

    void begin_container(layout_state& state, const container_descriptor& desc);
    void end_container(layout_state& state);

    void add_text(layout_state& state, const text_descriptor& desc);

    // Measures the width of a text string in pixels for the given font.
    f32 measure_text_width(layout_state& state, const char* text, font_id font, u16 fontSize);

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

        container_builder&& background_color(const color& color) &&
        {
            m_desc.backgroundColor = color;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& animation(const animation_config& config) &&
        {
            m_desc.animation = config;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& floating(const floating_config& config) &&
        {
            m_desc.floating = config;
            m_desc.isFloating = true;
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

        container_builder&& align(alignment a) &&
        {
            m_desc.alignment = a;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& align_x(alignment_x x) &&
        {
            m_desc.alignment.x = x;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& align_y(alignment_y y) &&
        {
            m_desc.alignment.y = y;
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

        container_builder&& width(const sizing& s) &&
        {
            m_desc.width = s;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& height(const sizing& s) &&
        {
            m_desc.height = s;
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
