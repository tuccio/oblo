#include <oblo/ui/game/ui.hpp>

#include <oblo/core/algorithm/fill.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ui/font.hpp>

#include <freetype/freetype.h>

namespace oblo::ui
{
    namespace
    {
        template <typename T>
        FT_Library to_freetype(T* freetype)
        {
            return std::bit_cast<FT_Library>(freetype);
        }
    }

    context::~context()
    {
        shutdown();
    }

    bool context::init()
    {
        FT_Library freetype;

        if (FT_Init_FreeType(&freetype))
        {
            return false;
        }

        m_freetype = std::bit_cast<freetype_lib*>(freetype);
        m_layout = create_state();

        return false;
    }

    void context::shutdown()
    {
        if (m_layout)
        {
            destroy_state(m_layout);
        }

        if (m_freetype)
        {
            FT_Done_FreeType(to_freetype(m_freetype));
        }
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
        // hit_test returns the topmost element, so clicks can only hit those.
        m_hoveredId = ui::hit_test(*m_layout, m_mousePosition);
        m_pressedId = mouse_clicked_this_frame(mouse_key::left)
            ? ui::hit_test(*m_layout, mouse_click_position(mouse_key::left))
            : layout_id{};

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

    const font* context::resolve_font_or_default(font_id id)
    {
        if (id)
        {
            return m_fonts.try_find(id);
        }

        return m_fontStack.empty() ? nullptr : resolve_font_or_default(m_fontStack.front());
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

    bool button(context& ctx, layout_id id, hashed_string_view label, const button_style& style)
    {
        const bool active = ctx.is_active(id);

        const bool hovered = ctx.is_hovered(id);
        const color bg = active ? style.activeColor : (hovered ? style.hoverColor : style.idleColor);

        const container_descriptor desc{
            .elementId = id,
            .direction = layout_direction::left_to_right,
            .width = style.width,
            .height = style.height,
            .backgroundColor = bg,
            .cornerRadius = vec4::splat(style.cornerRadius),
            .padding = style.padding,
        };

        ui::begin_container(ctx.get_layout(), desc);

        const font* font = ctx.resolve_font_or_default(style.font);
        OBLO_ASSERT_ONCE(font);

        if (font)
        {
            add_text(ctx.get_layout(),
                {
                    .text = label,
                    .font = font,
                    .color = style.textColor,
                });
        }

        ui::end_container(ctx.get_layout());

        return ctx.was_clicked(id);
    }

    void label(context& ctx, layout_id id, hashed_string_view text, const label_style& style)
    {
        const container_descriptor desc{
            .elementId = id,
            .direction = layout_direction::left_to_right,
            .width = style.width,
            .height = style.height,
            .padding = style.padding,
        };

        ui::begin_container(ctx.get_layout(), desc);

        const font* font = ctx.resolve_font_or_default(style.font);
        OBLO_ASSERT_ONCE(font);

        if (font)
        {
            add_text(ctx.get_layout(),
                {
                    .text = text,
                    .font = font,
                    .color = style.textColor,
                });
        }

        ui::end_container(ctx.get_layout());
    }

    bool checkbox(context& ctx, layout_id id, bool& checked, hashed_string_view text, const checkbox_style& style)
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

        const font* font = ctx.resolve_font_or_default(style.font);
        OBLO_ASSERT_ONCE(font);

        if (font)
        {
            add_text(ctx.get_layout(),
                {
                    .text = text,
                    .font = font,
                    .color = style.textColor,
                });
        }

        const bool wasClicked = ctx.was_clicked(id);

        if (wasClicked)
        {
            checked = !checked;
        }

        return wasClicked;
    }
}
