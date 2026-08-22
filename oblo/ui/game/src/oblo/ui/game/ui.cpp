#include <oblo/ui/game/ui.hpp>

#include <oblo/core/utility.hpp>

namespace oblo::ui::game
{
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
            .backgroundColor = style.backgroundColor,
            .cornerRadius = vec4::splat(style.cornerRadius),
            .childGap = style.gap,
            .padding = style.padding,
        };

        return begin_container(ctx, id, desc);
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
            .backgroundColor = bg,
            .cornerRadius = vec4::splat(style.cornerRadius),
            .padding = style.padding,
        };

        ui::begin_container(ctx.get_layout(), desc);
        ui::end_container(ctx.get_layout());

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
    }

    bool checkbox(context& ctx, layout_id id, bool& checked, string_view text, const checkbox_style& style)
    {
        ctx.begin_interaction(id);

        const auto container = container_builder{}.width(fit_size()).height(fit_size()).build(ctx.get_layout());

        {
            const auto box = container_builder{}
                                 .id(id)
                                 .width(fixed_size(style.boxSize))
                                 .height(fixed_size(style.boxSize))
                                 .background_color(style.boxColor)
                                 .build(ctx.get_layout());

            if (checked)
            {
                const auto check = container_builder{}
                                       .width(percent_size(.75f))
                                       .height(percent_size(.75f))
                                       .background_color(style.checkColor)
                                       .build(ctx.get_layout());
            }
        }

        // TODO: Add test instead of filler
        const auto textFillerBox = container_builder{}
                                       .width(fixed_size(f32(text.size32()) * .5f * style.fontHeight))
                                       .height(fixed_size(style.fontHeight))
                                       .build(ctx.get_layout());

        const bool wasClicked = ctx.was_clicked(id);

        if (wasClicked)
        {
            checked = !checked;
        }

        return wasClicked;
    }
}
