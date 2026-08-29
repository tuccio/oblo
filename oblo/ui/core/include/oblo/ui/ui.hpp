#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/core/span.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/time/time.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/input/input_event.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/ui/forward.hpp>
#include <oblo/ui/layout.hpp>
#include <oblo/ui/texture.hpp>

namespace oblo::ui
{
    struct font;
    using font_id = h16<font>;

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

    struct draw_command
    {
        rect bounds;

        color fill;
        vec4 cornerRadius;

        // When valid, the command draws a textured quad (e.g. a glyph) sampling the given
        // atlas texture. The uvRect selects the sub-rectangle of the atlas in normalized
        // coordinates. When invalid the command is a solid (rounded) rectangle.
        h32<texture> texture;
        vec4 uvRect;
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

        alignment alignment{alignment::top_left()};

        // Optional transition/enter/exit animation for this panel.
        animation_config animation{};
    };

    struct button_style
    {
        color idleColor{0.25f, 0.25f, 0.30f, 1.f};
        color hoverColor{0.35f, 0.35f, 0.42f, 1.f};
        color activeColor{0.45f, 0.45f, 0.55f, 1.f};
        color textColor{1.f, 1.f, 1.f, 1.f};
        f32 cornerRadius{4.f};
        padding padding{10.f, 10.f, 6.f, 6.f};

        sizing width{fit_size()};
        sizing height{fit_size()};

        font_id font{};
        u16 fontSize{};
    };

    struct label_style
    {
        color textColor{1.f, 1.f, 1.f, 1.f};
        padding padding{2.f, 2.f, 2.f, 2.f};

        sizing width{fit_size()};
        sizing height{fit_size()};

        font_id font{};
        u16 fontSize{};
    };

    struct checkbox_style
    {
        color boxColor{0.25f, 0.25f, 0.30f, 1.f};
        color checkColor{0.40f, 0.50f, 0.90f, 1.f};
        color textColor{1.f, 1.f, 1.f, 1.f};
        f32 cornerRadius{3.f};
        f32 boxSize{18.f};
        f32 gap{8.f};
        padding padding{4.f, 4.f, 4.f, 4.f};

        font_id font{};
        u16 fontSize{};
    };

    struct slider_style
    {
        color trackColor{0.20f, 0.20f, 0.25f, 1.f};
        color fillColor{0.40f, 0.50f, 0.90f, 1.f};
        color handleColor{1.f, 1.f, 1.f, 1.f};
        f32 cornerRadius{10.f};
        padding padding{4.f, 4.f, 4.f, 4.f};
    };

    struct font_state
    {
        font_id font;
        u16 fontSize;
    };

    class context
    {
    public:
        context();
        context(const context&) = delete;
        context(context&&) noexcept = delete;
        ~context();

        context& operator=(const context&) = delete;
        context& operator=(context&&) noexcept = delete;

        bool init();
        void shutdown();

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

        bool is_active(layout_id id) const;
        bool is_hovered(layout_id id) const;
        bool was_clicked(layout_id id) const;

        span<const texture_command> get_texture_commands() const;

        // Atlas textures generated from rendered glyphs, with their CPU-side pixel data.
        // Valid for as long as the context is alive.
        span<const texture> get_textures() const;

        // Draw commands generated from the layout at end_frame, in paint order.
        span<const draw_command> get_draw_commands() const;

        const vec2& mouse_position() const
        {
            return m_mousePosition;
        }

        bool mouse_clicked_this_frame(mouse_key key) const
        {
            return m_clickedThisFrame.contains(key);
        }

        const vec2& mouse_click_position(mouse_key key) const
        {
            return m_mouseClickPosition[u32(key)];
        }

        bool mouse_released_this_frame(mouse_key key) const
        {
            return m_releasedThisFrame.contains(key);
        }

        bool mouse_down_this_frame(mouse_key key) const
        {
            return m_mouseDown.contains(key);
        }

        void push_font(font_id font, u16 fontSize)
        {
            m_fontStack.emplace_back(font, fontSize);
        }

        void pop_font()
        {
            m_fontStack.pop_back();
        }

        font_state get_current_font() const
        {
            return m_fontStack.empty() ? font_state{} : m_fontStack.back();
        }

    private:
        bool try_render_rect(layout_id id, rect& out) const;

        struct texture_storage_impl;

    private:
        layout_state* m_layout{};
        unique_ptr<texture_storage_impl> m_textureStorage;
        vec2 m_mousePosition{};
        vec2 m_mouseClickPosition[u32(mouse_key::enum_max)]{};
        flags<mouse_key> m_mouseDown{};
        flags<mouse_key> m_clickedThisFrame{};
        flags<mouse_key> m_releasedThisFrame{};
        layout_id m_hoveredId{};
        layout_id m_pressedId{};
        layout_id m_activeId{};
        layout_id m_itemClickedThisFrame[u32(mouse_key::enum_max)]{};
        dynamic_array<font_state> m_fontStack;
        dynamic_array<draw_command> m_drawCommands;
    };

    class [[nodiscard]] panel_scope
    {
    public:
        panel_scope() = default;

        panel_scope(const panel_scope&) = delete;
        panel_scope& operator=(const panel_scope&) = delete;
        panel_scope& operator=(panel_scope&&) = delete;

        panel_scope(panel_scope&& o) noexcept : m_ctx{o.m_ctx}
        {
            o.m_ctx = nullptr;
        }

        explicit panel_scope(context& ctx) noexcept : m_ctx{&ctx} {}

        ~panel_scope();

    private:
        context* m_ctx{};
    };

    panel_scope panel(context& ctx, layout_id id, const panel_style& style = {});

    bool button(context& ctx, layout_id id, hashed_string_view text, const button_style& style = {});

    void label(context& ctx, layout_id id, hashed_string_view text, const label_style& style = {});

    bool checkbox(context& ctx, layout_id id, bool& checked, hashed_string_view text, const checkbox_style& style = {});
}
