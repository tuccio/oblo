#include <oblo/ui/game/ui.hpp>

#include <oblo/core/algorithm/fill.hpp>
#include <oblo/core/utility.hpp>

namespace oblo::ui
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
        fill(std::begin(m_itemClickedThisFrame), std::end(m_itemClickedThisFrame), {});

        m_clickedThisFrame = {};
        m_releasedThisFrame = {};

        for (const auto& e : events)
        {
            switch (e.kind)
            {
            case input_event_kind::mouse_move:
                m_mousePosition = {e.mouseMove.x, e.mouseMove.y};
                break;

            case input_event_kind::mouse_press:
                m_mouseDown.set(e.mousePress.key);
                m_clickedThisFrame.set(e.mousePress.key);
                m_mouseClickPosition[u32(e.mousePress.key)] = {e.mousePress.x, e.mousePress.y};
                m_mousePosition = {e.mousePress.x, e.mousePress.y};
                break;

            case input_event_kind::mouse_release:
                m_mouseDown.unset(e.mousePress.key);
                m_releasedThisFrame.set(e.mouseRelease.key);
                m_mousePosition = {e.mousePress.x, e.mousePress.y};
                break;

            default:
                break;
            }
        }

        ui::begin_frame(*m_layout, dt);
        ui::set_layout_size(*m_layout, layoutSize);

        // Resolve input once per frame against the previous frame's resolved geometry.
        // hit_test returns the topmost element, so clicks can't fall through to elements
        // drawn underneath, and each widget just compares its id (O(1) per widget).
        m_hoveredId = ui::hit_test(*m_layout, m_mousePosition);
        m_pressedId = mouse_clicked_this_frame(mouse_key::left)
            ? ui::hit_test(*m_layout, mouse_click_position(mouse_key::left))
            : layout_id{};

        // Only the topmost element under the click can become active, preventing clicks
        // from leaking to elements drawn underneath it.
        if (mouse_clicked_this_frame(mouse_key::left) && m_activeId == layout_id{} && m_pressedId != layout_id{})
        {
            m_activeId = m_pressedId;
        }

        if (mouse_released_this_frame(mouse_key::left) && m_activeId != layout_id{})
        {
            m_itemClickedThisFrame[u32(mouse_key::left)] = m_activeId;
            m_activeId = layout_id{};
        }
    }

    void context::end_frame()
    {
        ui::end_frame(*m_layout);
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

    bool context::is_active(layout_id id) const
    {
        return m_activeId == id;
    }

    bool context::is_hovered(layout_id id) const
    {
        return m_hoveredId == id;
    }

    bool context::was_clicked(layout_id id) const
    {
        return m_itemClickedThisFrame[u32(mouse_key::left)] == id;
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

        ui::begin_container(ctx.get_layout(), desc);

        return panel_scope{ctx};
    }

    bool button(context& ctx, layout_id id, string_view label, const button_style& style)
    {
        const vec2 textSize = ctx.measure(label, style.fontHeight);

        const f32 w = textSize.x + style.padding.left + style.padding.right;
        const f32 h = max(textSize.y, style.fontHeight) + style.padding.top + style.padding.bottom;

        const bool active = ctx.is_active(id);

        const bool hovered = ctx.is_hovered(id);
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
        const auto container = container_builder{}.width(fit_size()).height(fit_size()).build(ctx.get_layout());

        {
            const auto box = container_builder{}
                                 .id(id)
                                 .width(fixed_size(style.boxSize))
                                 .height(fixed_size(style.boxSize))
                                 .background_color(style.boxColor)
                                 .align(alignment::center())
                                 .build(ctx.get_layout());

            if (checked)
            {
                const auto check = container_builder{}
                                       .width(percent_size(.55f))
                                       .height(percent_size(.55f))
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
