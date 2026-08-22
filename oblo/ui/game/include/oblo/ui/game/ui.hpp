#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/input/input_event.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/ui/forward.hpp>
#include <oblo/ui/layout.hpp>

namespace oblo::ui::game
{
    struct draw_rect
    {
        rect bounds;
        color fill;
        vec4 cornerRadius;
    };

    struct draw_text
    {
        rect bounds;
        color color;
        f32 fontHeight;
        string_view text;
    };

    using measure_text_fn = function_ref<vec2(string_view text, f32 fontHeight)>;
    struct draw_intent
    {
        layout_id id;
        rect local;
        color fill;
        vec4 cornerRadius;
        string_view text;
        color textColor;
        f32 fontHeight;
        bool hasText;
    };

    struct panel_style
    {
        color backgroundColor{0.15f, 0.15f, 0.18f, 1.f};
        f32 cornerRadius{6.f};
        padding padding{8.f, 8.f, 8.f, 8.f};
        layout_direction direction{layout_direction::top_to_bottom};
        f32 gap{4.f};
        sizing width{fit_size()};
        sizing height{fit_size()};
    };

    struct button_style
    {
        color idleColor{0.25f, 0.25f, 0.30f, 1.f};
        color hoverColor{0.35f, 0.35f, 0.42f, 1.f};
        color activeColor{0.45f, 0.45f, 0.55f, 1.f};
        color textColor{1.f, 1.f, 1.f, 1.f};
        f32 cornerRadius{4.f};
        padding padding{10.f, 10.f, 6.f, 6.f};
        f32 fontHeight{16.f};
    };

    struct label_style
    {
        color textColor{1.f, 1.f, 1.f, 1.f};
        f32 fontHeight{16.f};
        padding padding{2.f, 2.f, 2.f, 2.f};
    };

    struct checkbox_style
    {
        color boxColor{0.25f, 0.25f, 0.30f, 1.f};
        color checkColor{0.40f, 0.50f, 0.90f, 1.f};
        color textColor{1.f, 1.f, 1.f, 1.f};
        f32 cornerRadius{3.f};
        f32 boxSize{18.f};
        f32 gap{8.f};
        f32 fontHeight{16.f};
        padding padding{4.f, 4.f, 4.f, 4.f};
    };

    struct slider_style
    {
        color trackColor{0.20f, 0.20f, 0.25f, 1.f};
        color fillColor{0.40f, 0.50f, 0.90f, 1.f};
        color handleColor{1.f, 1.f, 1.f, 1.f};
        f32 cornerRadius{10.f};
        padding padding{4.f, 4.f, 4.f, 4.f};
    };

    class context
    {
    public:
        context();
        ~context();

        void set_measure_text(measure_text_fn fn)
        {
            m_measureText = fn;
        }

        void begin_frame(span<const input_event> events, time dt, vec2 layoutSize);
        void end_frame();

        OBLO_FORCEINLINE layout_state& get_layout()
        {
            return *m_layout;
        }

        OBLO_FORCEINLINE span<const layout_element> get_layout_elements() const
        {
            return get_elements(*m_layout);
        }

        vec2 measure(string_view text, f32 fontHeight) const;

        bool is_hovered(layout_id id) const;
        bool is_active(layout_id id) const;
        bool was_clicked(layout_id id) const;

        bool begin_interaction(layout_id id);

        const rect* find_prev_rect(layout_id id) const;

        const vec2& mouse_position() const
        {
            return m_mousePosition;
        }

    private:
        bool try_render_rect(layout_id id, rect& out) const;

    private:
        layout_state* m_layout{};
        vec2 m_mousePosition{};
        bool m_mouseDown{};
        bool m_pressedThisFrame{};
        bool m_releasedThisFrame{};
        measure_text_fn m_measureText{};
        dynamic_array<layout_element> m_prevElements;
        layout_id m_activeId{};
        layout_id m_clickedThisFrame{};
    };

    class panel_scope
    {
    public:
        panel_scope() = default;

        panel_scope(const panel_scope&) = delete;
        panel_scope& operator=(const panel_scope&) = delete;
        panel_scope& operator=(panel_scope&&) = delete;

        panel_scope(panel_scope&& o) noexcept : m_ctx{o.m_ctx}, m_id{o.m_id}, m_hovered{o.m_hovered}
        {
            o.m_ctx = nullptr;
        }

        ~panel_scope();

        bool hovered() const
        {
            return m_hovered;
        }

        layout_id id() const
        {
            return m_id;
        }

    private:
        friend panel_scope begin_container(context& ctx, layout_id id, const container_descriptor& desc);
        friend panel_scope begin_panel(context& ctx, layout_id id, const panel_style& style);

        context* m_ctx{};
        layout_id m_id{};
        bool m_hovered{};
    };

    panel_scope begin_container(context& ctx, layout_id id, const container_descriptor& desc);
    panel_scope begin_panel(context& ctx, layout_id id, const panel_style& style = {});

    bool button(context& ctx, layout_id id, string_view label, const button_style& style = {});

    void label(context& ctx, layout_id id, string_view text, const label_style& style = {});

    bool checkbox(context& ctx, layout_id id, bool& checked, string_view text, const checkbox_style& style = {});
}
