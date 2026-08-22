#include <oblo/ui/game/ui.hpp>

#include <oblo/core/utility.hpp>

#include <cstring>

namespace oblo::ui::game
{
    namespace
    {
        f32 clamp01(f32 x)
        {
            return x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
        }
    }

    context::context()
    {
        m_layout = create_state();
    }

    context::~context()
    {
        destroy_state(m_layout);
    }

    void context::begin_frame(span<const input_event> events, time dt, vec2 layoutSize)
    {
        m_rects.clear();
        m_texts.clear();
        m_intents.clear();

        m_clickedThisFrame = layout_id{};
        m_pressedThisFrame = false;
        m_releasedThisFrame = false;

        for (const auto& e : events)
        {
            switch (e.kind)
            {
            case input_event_kind::mouse_move:
                m_mousePosition = {e.mouseMove.x, e.mouseMove.y};
                break;
            case input_event_kind::mouse_press:
                if (e.mousePress.key == mouse_key::left)
                {
                    m_mouseDown = true;
                    m_pressedThisFrame = true;
                }
                break;
            case input_event_kind::mouse_release:
                if (e.mouseRelease.key == mouse_key::left)
                {
                    m_mouseDown = false;
                    m_releasedThisFrame = true;
                }
                break;
            default:
                break;
            }
        }

        ui::begin_frame(*m_layout, dt);
        ui::set_layout_size(*m_layout, layoutSize);
    }

    void context::end_frame()
    {
        oblo::ui::end_frame(*m_layout);

        for (const auto& it : m_intents)
        {
            rect wr;

            if (!try_render_rect(it.id, wr))
            {
                continue;
            }

            const rect r =
                it.local.width < 0.f ? wr : rect{wr.x + it.local.x, wr.y + it.local.y, it.local.width, it.local.height};

            if (it.hasText)
            {
                m_texts.push_back({r, it.textColor, it.fontHeight, it.text});
            }
            else
            {
                m_rects.push_back({r, it.fill, it.cornerRadius});
            }
        }

        m_intents.clear();

        const std::span elements = get_elements(*m_layout);
        m_prevElements.assign(elements.begin(), elements.end());
    }

    vec2 context::measure(string_view text, f32 fontHeight) const
    {
        if (m_measureText)
        {
            return m_measureText(text, fontHeight);
        }

        const u32 len = text.size32();
        return {.5f * f32(len) * fontHeight, fontHeight};
    }

    bool context::is_hovered(layout_id id) const
    {
        const rect* const r = find_prev_rect(id);
        return r != nullptr && r->contains(m_mousePosition);
    }

    bool context::is_active(layout_id id) const
    {
        return m_activeId == id;
    }

    bool context::begin_interaction(layout_id id)
    {
        const bool hovered = is_hovered(id);

        if (hovered && m_pressedThisFrame && m_activeId == layout_id{})
        {
            m_activeId = id;
        }

        if (m_releasedThisFrame && m_activeId == id)
        {
            m_clickedThisFrame = id;
            m_activeId = layout_id{};
        }

        return hovered;
    }

    bool context::was_clicked(layout_id id) const
    {
        return m_clickedThisFrame == id;
    }

    const rect* context::find_prev_rect(layout_id id) const
    {
        for (const auto& e : m_prevElements)
        {
            if (e.elementId == id)
            {
                return &e.targetRect;
            }
        }

        return nullptr;
    }

    void context::emit_rect(layout_id id, const color& fill, vec4 cornerRadius, const rect& local)
    {
        m_intents.push_back({
            .id = id,
            .local = local,
            .fill = fill,
            .cornerRadius = cornerRadius,
            .hasText = false,
        });
    }

    void context::emit_text(layout_id id, string_view text, const color& c, f32 fontHeight, const rect& local)
    {
        m_intents.push_back({
            .id = id,
            .local = local,
            .text = text,
            .textColor = c,
            .fontHeight = fontHeight,
            .hasText = true,
        });
    }

    bool context::try_render_rect(layout_id id, rect& out) const
    {
        for (const auto& e : get_elements(*m_layout))
        {
            if (e.elementId == id)
            {
                out = e.animated ? e.animated->boundingBox : e.targetRect;
                return true;
            }
        }

        return false;
    }

    panel_scope::~panel_scope()
    {
        if (m_ctx)
        {
            end_container(m_ctx->get_layout());
        }
    }

    panel_scope begin_container(context& ctx, layout_id id, const container_descriptor& desc)
    {
        container_descriptor d = desc;
        d.elementId = id;

        ui::begin_container(ctx.get_layout(), d);

        panel_scope scope;
        scope.m_ctx = &ctx;
        scope.m_id = id;
        scope.m_hovered = ctx.is_hovered(id);
        return scope;
    }

    panel_scope begin_panel(context& ctx, layout_id id, const panel_style& style)
    {
        const container_descriptor desc{
            .elementId = id,
            .direction = style.direction,
            .width = style.width,
            .height = style.height,
            .cornerRadius = vec4::splat(style.cornerRadius),
            .childGap = style.gap,
            .padding = style.padding,
        };

        auto scope = begin_container(ctx, id, desc);

        ctx.emit_rect(id, style.backgroundColor, vec4::splat(style.cornerRadius));

        return scope;
    }

    bool button(context& ctx, layout_id id, string_view label, const button_style& style)
    {
        const vec2 textSize = ctx.measure(label, style.fontHeight);

        const f32 w = textSize.x + style.padding.left + style.padding.right;
        const f32 h = max(textSize.y, style.fontHeight) + style.padding.top + style.padding.bottom;

        const bool hovered = ctx.begin_interaction(id);
        const bool active = ctx.is_active(id);

        const color bg = active ? style.activeColor : (hovered ? style.hoverColor : style.idleColor);

        const container_descriptor desc{
            .elementId = id,
            .direction = layout_direction::left_to_right,
            .width = fixed_size(w),
            .height = fixed_size(h),
            .cornerRadius = vec4::splat(style.cornerRadius),
            .padding = style.padding,
        };

        ui::begin_container(ctx.get_layout(), desc);
        ui::end_container(ctx.get_layout());

        ctx.emit_rect(id, bg, vec4::splat(style.cornerRadius));
        ctx.emit_text(id, label, style.textColor, style.fontHeight);

        return ctx.was_clicked(id);
    }

    void label(context& ctx, layout_id id, string_view text, const label_style& style)
    {
        const vec2 textSize = ctx.measure(text, style.fontHeight);

        const f32 w = textSize.x + style.padding.left + style.padding.right;
        const f32 h = textSize.y + style.padding.top + style.padding.bottom;

        container_descriptor desc{};
        desc.elementId = id;
        desc.direction = layout_direction::left_to_right;
        desc.padding = style.padding;
        desc.width = fixed_size(w);
        desc.height = fixed_size(h);

        ui::begin_container(ctx.get_layout(), desc);
        ui::end_container(ctx.get_layout());

        ctx.emit_text(id, text, style.textColor, style.fontHeight);
    }

    bool checkbox(context& ctx, layout_id id, bool& checked, string_view text, const checkbox_style& style)
    {
        const vec2 textSize = ctx.measure(text, style.fontHeight);

        const f32 w = style.boxSize + style.gap + textSize.x + style.padding.left + style.padding.right;
        const f32 h = max(style.boxSize, textSize.y) + style.padding.top + style.padding.bottom;

        ctx.begin_interaction(id);

        container_descriptor desc{};
        desc.elementId = id;
        desc.direction = layout_direction::left_to_right;
        desc.padding = style.padding;
        desc.width = fixed_size(w);
        desc.height = fixed_size(h);

        oblo::ui::begin_container(ctx.get_layout(), desc);
        oblo::ui::end_container(ctx.get_layout());

        const f32 boxY = (h - style.boxSize) * .5f;

        ctx.emit_rect(id,
            style.boxColor,
            vec4::splat(style.cornerRadius),
            rect{
                style.padding.left,
                boxY,
                style.boxSize,
                style.boxSize,
            });

        if (checked)
        {
            const f32 inset = style.boxSize * 0.28f;

            ctx.emit_rect(id,
                style.checkColor,
                vec4::splat(style.cornerRadius * .5f),
                rect{
                    style.padding.left + inset,
                    boxY + inset,
                    style.boxSize - 2.f * inset,
                    style.boxSize - 2.f * inset,
                });
        }

        const f32 textX = style.padding.left + style.boxSize + style.gap;
        const f32 textY = (h - textSize.y) * .5f;

        ctx.emit_text(id, text, style.textColor, style.fontHeight, rect{textX, textY, textSize.x, textSize.y});

        if (ctx.was_clicked(id))
        {
            checked = !checked;
            return true;
        }

        return false;
    }

    bool slider(context& ctx, layout_id id, f32& value, f32 min, f32 max, const slider_style& style)
    {
        const f32 w = style.width;
        const f32 h = style.height;

        const f32 oldValue = value;

        ctx.begin_interaction(id);

        if (ctx.is_active(id))
        {
            const rect* const track = ctx.find_prev_rect(id);

            if (track && track->width > 0.f)
            {
                const f32 trackX = style.padding.left;
                const f32 trackW = w - style.padding.left - style.padding.right;
                const f32 t = clamp01((ctx.mouse_position().x - (track->x + trackX)) / trackW);
                value = min + t * (max - min);
            }
        }

        const f32 t = clamp01((value - min) / (max - min));

        container_descriptor desc{};
        desc.elementId = id;
        desc.direction = layout_direction::left_to_right;
        desc.padding = style.padding;
        desc.width = fixed_size(w);
        desc.height = fixed_size(h);

        oblo::ui::begin_container(ctx.get_layout(), desc);
        oblo::ui::end_container(ctx.get_layout());

        const f32 trackX = style.padding.left;
        const f32 trackW = w - style.padding.left - style.padding.right;
        const f32 trackH = h - style.padding.top - style.padding.bottom;
        const f32 trackY = style.padding.top;

        const vec4 cornerRadius = vec4::splat(style.cornerRadius);

        ctx.emit_rect(id, style.trackColor, cornerRadius, rect{trackX, trackY, trackW, trackH});

        const f32 fillW = trackW * t;
        ctx.emit_rect(id, style.fillColor, cornerRadius, rect{trackX, trackY, fillW, trackH});

        const f32 handle = trackH;
        const f32 handleX = trackX + trackW * t - handle * .5f;
        ctx.emit_rect(id, style.handleColor, cornerRadius, rect{handleX, trackY, handle, trackH});

        return value != oldValue;
    }
}
