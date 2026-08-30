#include <oblo/ui/ui.hpp>

#include <oblo/ui/embedded/Archivo-Regular.ttf.h>
#include <oblo/ui/font.hpp>
#include <oblo/ui/layout_impl.hpp>
#include <oblo/ui/texture_storage.hpp>

#include <oblo/core/algorithm/fill.hpp>
#include <oblo/core/utility.hpp>

namespace oblo::ui
{
    namespace
    {
        constexpr u32 glyph_atlas_resolution = 2048;

        font_state resolve_font(const context& ctx, font_id id, u16 size)
        {
            return id ? font_state{id, size} : ctx.get_current_font();
        }
    }

    struct context::texture_storage_impl : texture_storage
    {
    };

    context::context() = default;

    context::~context()
    {
        shutdown();
    }

    bool context::init()
    {
        m_layout = create_state();

        if (m_layout)
        {
            const expected<font_id> defaultFont = load_font_from_memory(*m_layout, as_bytes(span{Archivo_Regular_ttf}));

            if (!defaultFont)
            {
                destroy_state(m_layout);
                m_layout = nullptr;
                return false;
            }

            constexpr u16 defaultFontSize = 14;
            push_font(*defaultFont, defaultFontSize);
        }

        m_textureStorage = allocate_unique<texture_storage_impl>();

        // Link the font cache to our texture storage so glyphs get rasterized into atlas
        // textures, and pick a default atlas resolution.
        m_layout->fonts.textures = m_textureStorage.get();
        m_layout->fonts.textureAtlasResolution = glyph_atlas_resolution;

        return m_layout != nullptr;
    }

    void context::shutdown()
    {
        if (m_layout)
        {
            destroy_state(m_layout);
            m_layout = nullptr;
        }

        m_textureStorage.reset();
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
                m_mouseDown.unset(e.mouseRelease.key);
                m_releasedThisFrame.set(e.mouseRelease.key);
                m_mousePosition = {e.mouseRelease.x, e.mouseRelease.y};
                break;

            default:
                break;
            }
        }

        m_textureStorage->begin_frame();

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

        m_drawCommands.clear();

        for (const auto& e : get_elements(*m_layout))
        {
            if (e.kind == layout_element_kind::container)
            {
                auto& cmd = m_drawCommands.push_back_default();

                cmd = {
                    .bounds = e.get_current_rect(),
                    .fill = e.get_current_background_color(),
                    .cornerRadius = e.get_current_corner_radius(),
                };
            }
            else
            {
                const FT_Face face = m_layout->fonts.find_font(e.data.text.font);

                if (!face)
                {
                    continue;
                }

                const f32 ascent = f32(face->size->metrics.ascender >> 6);
                const rect textRect = e.get_current_rect();

                f32 penX = textRect.x;
                const f32 penY = textRect.y;

                for (const u32 glyph : e.data.text.glyphs)
                {
                    const expected rendered =
                        m_layout->fonts.get_rendered_glyph({e.data.text.font, e.data.text.fontSize, glyph}, face);

                    if (!rendered)
                    {
                        continue;
                    }

                    const f32 x = penX + rendered->bearingX;
                    const f32 y = penY + (ascent - rendered->bearingY);

                    const vec2 uvMin{
                        rendered->posX / f32(rendered->atlasWidth),
                        rendered->posY / f32(rendered->atlasHeight),
                    };

                    const vec2 uvSize{
                        rendered->width / f32(rendered->atlasWidth),
                        rendered->height / f32(rendered->atlasHeight),
                    };

                    auto& cmd = m_drawCommands.push_back_default();

                    cmd = {
                        .bounds = rect{x, y, f32(rendered->width), f32(rendered->height)},
                        .fill = e.data.text.color,
                        .cornerRadius = {},
                        .texture = rendered->atlas,
                        .uvRect = vec4{uvMin.x, uvMin.y, uvSize.x, uvSize.y},
                    };

                    penX += rendered->advanceX;
                }
            }
        }

        // Batch every glyph written this frame into one upload command per atlas.
        m_layout->fonts.flush_atlas_uploads();
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

    span<const texture_command> context::get_texture_commands() const
    {
        return m_textureStorage->commands;
    }

    span<const texture> context::get_textures() const
    {
        return m_textureStorage->get_textures();
    }

    span<const draw_command> context::get_draw_commands() const
    {
        return m_drawCommands;
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

    panel_scope panel(context& ctx, layout_id id, const panel_style& style)
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
            .alignment = style.alignment,
            .animation = style.animation,
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

        const font_state currentFont = resolve_font(ctx, style.font, style.fontSize);

        add_text(ctx.get_layout(),
            {
                .text = label,
                .color = style.textColor,
                .font = currentFont.font,
                .fontSize = currentFont.fontSize,
            });

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

        const font_state currentFont = resolve_font(ctx, style.font, style.fontSize);

        add_text(ctx.get_layout(),
            {
                .text = text,
                .color = style.textColor,
                .font = currentFont.font,
                .fontSize = currentFont.fontSize,
            });

        ui::end_container(ctx.get_layout());
    }

    bool checkbox(context& ctx, layout_id id, bool& checked, hashed_string_view text, const checkbox_style& style)
    {
        constexpr f32 gap = 8.f;

        const auto container = container_builder{}
                                   .id(id)
                                   .width(fit_size())
                                   .height(fit_size())
                                   .gap(gap)
                                   .align_y(alignment_y::center)
                                   .build(ctx.get_layout());

        {
            const auto box = container_builder{}
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

        const font_state currentFont = resolve_font(ctx, style.font, style.fontSize);

        add_text(ctx.get_layout(),
            {
                .text = text,
                .color = style.textColor,
                .font = currentFont.font,
                .fontSize = currentFont.fontSize,
            });

        const bool wasClicked = ctx.was_clicked(id);

        if (wasClicked)
        {
            checked = !checked;
        }

        return wasClicked;
    }

    bool radio_button(context& ctx, layout_id id, bool& selected, hashed_string_view text, const radio_style& style)
    {
        const auto container = container_builder{}
                                   .id(id)
                                   .width(fit_size())
                                   .height(fit_size())
                                   .direction(layout_direction::left_to_right)
                                   .gap(style.gap)
                                   .align_y(alignment_y::center)
                                   .build(ctx.get_layout());

        {
            const auto box = container_builder{}
                                 .width(fixed_size(style.boxSize))
                                 .height(fixed_size(style.boxSize))
                                 .background_color(selected ? style.checkColor : style.boxColor)
                                 .corner_radius(style.boxSize * 0.5f)
                                 .align(alignment::center())
                                 .build(ctx.get_layout());

            if (selected)
            {
                const auto dot = container_builder{}
                                     .width(percent_size(0.45f))
                                     .height(percent_size(0.45f))
                                     .background_color(style.boxColor)
                                     .corner_radius(style.boxSize * 0.25f)
                                     .build(ctx.get_layout());
            }
        }

        const font_state currentFont = resolve_font(ctx, style.font, style.fontSize);

        add_text(ctx.get_layout(),
            {
                .text = text,
                .color = style.textColor,
                .font = currentFont.font,
                .fontSize = currentFont.fontSize,
            });

        const bool wasClicked = ctx.was_clicked(id);

        if (wasClicked && !selected)
        {
            selected = true;
            return true;
        }

        return false;
    }

    radio_group_builder::radio_group_builder(context& ctx, layout_id& selected, const radio_style& style) :
        m_ctx{&ctx}, m_selected{&selected}, m_style{style}
    {
    }

    bool radio_group_builder::add_option(layout_id optionId, hashed_string_view text)
    {
        bool selected = *m_selected == optionId;

        if (radio_button(*m_ctx, optionId, selected, text, m_style))
        {
            *m_selected = optionId;
            return true;
        }

        return false;
    }

    combo_box_builder::combo_box_builder(
        context& ctx, layout_id id, hashed_string_view headerText, const combo_style& style) :
        m_ctx{&ctx}, m_id{id}, m_style{style}, m_open{ctx.is_popup_open(id)}
    {
        const bool headerActive = ctx.is_active(id);
        const bool headerHovered = ctx.is_hovered(id);
        const color hdrBg = headerActive ? style.activeColor : (headerHovered ? style.hoverColor : style.idleColor);

        const container_descriptor desc{
            .elementId = id,
            .direction = layout_direction::top_to_bottom,
            .width = style.width,
            .height = style.height,
            .backgroundColor = hdrBg,
            .cornerRadius = vec4::splat(style.cornerRadius),
            .childGap = 0.f,
            .padding = style.padding,
        };

        ui::begin_container(ctx.get_layout(), desc);

        const font_state currentFont = resolve_font(ctx, style.font, style.fontSize);

        add_text(ctx.get_layout(),
            {
                .text = headerText,
                .color = style.textColor,
                .font = currentFont.font,
                .fontSize = currentFont.fontSize,
            });

        if (ctx.was_clicked(id))
        {
            m_open = !m_open;
        }

        m_popupOpen = m_open;

        if (m_popupOpen)
        {
            const container_descriptor popupDesc{
                .direction = layout_direction::top_to_bottom,
                .width = style.width,
                .height = fit_size(),
                .backgroundColor = style.popupColor,
                .cornerRadius = vec4::splat(style.cornerRadius),
                .childGap = style.itemGap,
                .padding = {style.popupPadding, style.popupPadding, style.popupPadding, style.popupPadding},
                .floating =
                    floating_config{
                        .anchorId = id,
                        .anchorPoint = alignment::bottom_left(),
                        .selfPoint = alignment::top_left(),
                        .offset = {0.f, 4.f},
                        .zIndex = 100.f,
                    },
                .isFloating = true,
            };

            ui::begin_container(ctx.get_layout(), popupDesc);
        }
    }

    bool combo_box_builder::add_item(layout_id itemId, hashed_string_view text)
    {
        bool clicked = false;

        if (m_popupOpen)
        {
            const button_style itemButtonStyle{
                .idleColor = m_style.popupColor,
                .hoverColor = m_style.popupHoverColor,
                .activeColor = m_style.popupHoverColor,
                .textColor = m_style.popupTextColor,
                .cornerRadius = m_style.cornerRadius,
                .padding = m_style.padding,
                .width = percent_size(1.f),
                .font = m_style.font,
                .fontSize = m_style.fontSize,
            };

            if (button(*m_ctx, itemId, text, itemButtonStyle))
            {
                m_anyItemClicked = true;
                m_open = false;
                clicked = true;
            }
        }

        return clicked;
    }

    combo_box_builder::~combo_box_builder()
    {
        if (!m_ctx)
        {
            return;
        }

        if (m_popupOpen)
        {
            end_container(m_ctx->get_layout());
        }

        if (m_open && !m_anyItemClicked && m_ctx->mouse_released_this_frame(mouse_key::left) &&
            !m_ctx->was_clicked(m_id))
        {
            m_open = false;
        }

        m_ctx->set_popup_open(m_id, m_open);

        end_container(m_ctx->get_layout());
    }

    bool slider(context& ctx, layout_id id, f32& value, const slider_style& style, f32 min, f32 max)
    {
        const f32 range = max - min;
        const f32 t = range > 0.f ? (value - min) / range : 0.f;
        const f32 clampedT = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);

        constexpr f32 thinBarHeight = 7.f;
        constexpr f32 thinBarRadius = thinBarHeight * 0.5f;

        const auto track = container_builder{}
                               .id(id)
                               .width(style.width)
                               .height(fixed_size(style.trackHeight))
                               .direction(layout_direction::left_to_right)
                               .align_y(alignment_y::center)
                               .build(ctx.get_layout());

        {
            const auto trackBg = container_builder{}
                                     .width(percent_size(1.f))
                                     .height(fixed_size(thinBarHeight))
                                     .background_color(style.trackColor)
                                     .corner_radius(thinBarRadius)
                                     .build(ctx.get_layout());
        }

        {
            const auto fillBar = container_builder{}
                                     .width(percent_size(clampedT))
                                     .height(fixed_size(thinBarHeight))
                                     .background_color(style.fillColor)
                                     .corner_radius(thinBarRadius)
                                     .floating(floating_config{
                                         .anchorPoint = alignment::center_left(),
                                         .selfPoint = alignment::center_left(),
                                         .offset = {},
                                         .zIndex = 1.f,
                                     })
                                     .build(ctx.get_layout());
        }

        {
            const auto handleWrapper = container_builder{}
                                           .width(percent_size(clampedT))
                                           .height(percent_size(1.f))
                                           .floating({
                                               .anchorPoint = alignment::center_left(),
                                               .selfPoint = alignment::center_left(),
                                               .offset = {},
                                               // Z needs to be on top of the track
                                               .zIndex = 2.f, 
                                           })
                                           .direction(layout_direction::left_to_right)
                                           .align_x(alignment_x::right)
                                           .align_y(alignment_y::center)
                                           .build(ctx.get_layout());

            const auto handle = container_builder{}
                                    .width(fixed_size(style.handleSize))
                                    .height(fixed_size(style.handleSize))
                                    .background_color(style.handleColor)
                                    .corner_radius(style.handleSize * 0.5f)
                                    .build(ctx.get_layout());
        }

        bool changed = false;

        if (ctx.is_active(id))
        {
            rect trackRect{};

            if (ctx.get_last_frame_rect(id, trackRect))
            {
                const f32 localX = ctx.mouse_position().x - trackRect.x;
                const f32 newT = localX / (trackRect.width > 1e-3f ? trackRect.width : 1e-3f);
                const f32 newClampedT = newT < 0.f ? 0.f : (newT > 1.f ? 1.f : newT);
                const f32 newValue = min + newClampedT * range;

                if (newValue != value)
                {
                    value = newValue;
                    changed = true;
                }
            }
        }

        return changed;
    }
}
