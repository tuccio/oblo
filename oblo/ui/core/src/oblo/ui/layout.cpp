#include <oblo/ui/layout.hpp>
#include <oblo/ui/layout_impl.hpp>

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/string/utf.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/math/float.hpp>
#include <oblo/ui/font.hpp>

#include <freetype/freetype.h>

#include <cmath>
#include <limits>

namespace oblo::ui
{
    namespace
    {
        f32 resolve_axis_size(const sizing& s, f32 content, f32 parentSize)
        {
            switch (s.kind)
            {
            case sizing_kind::fixed:
                return max(s.sizing.fixed.size, 0.f);
            case sizing_kind::percentage:
                return parentSize * s.sizing.percentage.size;
            case sizing_kind::fit: {
                const f32 minSize = max(s.sizing.fit.min, 0.f);
                const f32 maxSize = s.sizing.fit.max <= 0.f ? std::numeric_limits<f32>::max() : s.sizing.fit.max;
                return min(maxSize, max(content, minSize));
            }
            default:
                return 0.f;
            }
        }

        constexpr f32 ease_out_bounce(f32 x)
        {
            constexpr f32 n1 = 7.5625f;
            constexpr f32 d1 = 2.75f;

            if (x < 1.f / d1)
            {
                return n1 * x * x;
            }
            else if (x < 2.f / d1)
            {
                const f32 x2 = x - 1.5f / d1;
                return n1 * x2 * x2 + 0.75f;
            }
            else if (x < 2.5f / d1)
            {
                const f32 x2 = x - 2.25f / d1;
                return n1 * x2 * x2 + 0.9375f;
            }
            else
            {
                const f32 x2 = x - 2.625f / d1;
                return n1 * x2 * x2 + 0.984375f;
            }
        }

        bool has_animation(const animation_config& cfg)
        {
            return !cfg.properties.is_empty();
        }

        vec2 measure_text(font_cache& fonts, font_id font, FT_Face face, u16 fontSize, std::span<const u32> glyphs)
        {
            OBLO_ASSERT(face && face == fonts.find_font(font));

            if (!face)
            {
                return {};
            }

            const f32 height = f32(face->size->metrics.height >> 6);
            f32 width = 0.f;

            for (const u32 glyphIndex : glyphs)
            {
                const auto glyph = fonts.get_or_add_glyph({font, fontSize, glyphIndex}, face);

                if (!glyph)
                {
                    continue;
                }

                width += glyph->advanceX;
            }

            return {width, height};
        }

        span<const u32> text_to_glyphs(bump_allocator& allocator, FT_Face face, string_view text)
        {
            if (text.empty() || !face)
            {
                return {};
            }

            const span glyphs = allocate_n_span<u32>(allocator, text.size());

            usize actualCount = 0;

            for (const char *it = text.data(), *end = text.data() + text.size(); it != end;)
            {
                const FT_ULong codepoint = utf8_next_codepoint(&it);

                const FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);

                if (glyphIndex == 0)
                {
                    continue;
                }

                glyphs[actualCount] = glyphIndex;
                ++actualCount;
            }

            return glyphs.subspan(0, actualCount);
        }

        void resolve_element(layout_state& state,
            u32 index,
            vec2 parentOrigin,
            vec2 parentInnerSize,
            f32 mainCursor,
            layout_direction parentDirection)
        {
            auto& elements = state.elements;
            auto& element = elements[index];

            // Percentage sizing is resolved against the parent's inner size (the parent's
            // padding has already been removed).
            const vec2 size{
                resolve_axis_size(element.width, element.contentSize.x, parentInnerSize.x),
                resolve_axis_size(element.height, element.contentSize.y, parentInnerSize.y),
            };

            vec2 pos = parentOrigin;

            if (parentDirection == layout_direction::left_to_right)
            {
                pos.x += mainCursor;
            }
            else
            {
                pos.y += mainCursor;
            }

            element.targetRect = {pos.x, pos.y, size.x, size.y};

            // Feed the animation system, parents before children.
            if (element.elementId != layout_id{} && element.kind == layout_element_kind::container &&
                has_animation(element.data.container.animation))
            {
                const animated_values target{
                    .boundingBox = element.targetRect,
                    .backgroundColor = element.data.container.backgroundColor,
                    .cornerRadius = element.data.container.cornerRadius,
                };

                const layout_id parentId =
                    element.parentIndex != invalid_index ? elements[element.parentIndex].elementId : layout_id{};

                element.animated = update_element(state,
                    element.elementId,
                    parentId,
                    element.targetRect.position(),
                    target,
                    element.data.container.animation);
            }

            if (element.kind == layout_element_kind::container && element.firstChild != invalid_index)
            {
                const container_layout_data& desc = element.data.container;

                // Position the children along this element's layout axis, inset by this
                // element's padding. The padding offsets the children, not the element itself.
                const vec2 innerSize = {
                    max(size.x - desc.padding.left - desc.padding.right, 0.f),
                    max(size.y - desc.padding.top - desc.padding.bottom, 0.f),
                };

                const vec2 childOrigin = element.targetRect.position() + vec2{desc.padding.left, desc.padding.top};

                const bool isHorizontal = desc.direction == layout_direction::left_to_right;

                // Resolve each child's final size against this element's inner size. Percentage
                // children are still 0 in targetRect at this point (they get expanded later, in
                // resolve_element), so they must be resolved here to measure and align correctly.
                auto resolve_child_size = [&](u32 child) -> vec2
                {
                    OBLO_ASSERT(element.kind == layout_element_kind::container);

                    const auto& cd = elements[child];

                    return {
                        resolve_axis_size(cd.width, elements[child].contentSize.x, innerSize.x),
                        resolve_axis_size(cd.height, elements[child].contentSize.y, innerSize.y),
                    };
                };

                // Measure the children's content extent along the main axis so the group can be aligned as a whole
                f32 contentMain = 0.f;
                u32 childCount = 0;

                for (u32 child = element.firstChild; child != invalid_index; child = elements[child].nextSibling)
                {
                    const vec2 childSize = resolve_child_size(child);
                    contentMain += isHorizontal ? childSize.x : childSize.y;
                    ++childCount;
                }

                if (childCount > 1)
                {
                    contentMain += (childCount - 1) * desc.childGap;
                }

                // On-axis alignment: shift the whole child group along the main axis.
                const f32 innerMain = isHorizontal ? innerSize.x : innerSize.y;
                const f32 extraSpace = max(0.f, innerMain - contentMain);

                f32 mainOffset = 0.f;

                if (isHorizontal)
                {
                    switch (desc.alignment.x)
                    {
                    case alignment_x::center:
                        mainOffset = extraSpace * 0.5f;
                        break;
                    case alignment_x::right:
                        mainOffset = extraSpace;
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    switch (desc.alignment.y)
                    {
                    case alignment_y::center:
                        mainOffset = extraSpace * 0.5f;
                        break;
                    case alignment_y::bottom:
                        mainOffset = extraSpace;
                        break;
                    default:
                        break;
                    }
                }

                f32 cursor = mainOffset;

                for (u32 child = element.firstChild; child != invalid_index; child = elements[child].nextSibling)
                {
                    const vec2 childSize = resolve_child_size(child);
                    const f32 childMain = isHorizontal ? childSize.x : childSize.y;
                    const f32 childCross = isHorizontal ? childSize.y : childSize.x;

                    // Cross-axis alignment: shift each child along the cross axis independently.
                    const f32 innerCross = isHorizontal ? innerSize.y : innerSize.x;
                    const f32 whiteSpace = max(0.f, innerCross - childCross);

                    f32 crossOffset = 0.f;

                    if (isHorizontal)
                    {
                        switch (desc.alignment.y)
                        {
                        case alignment_y::center:
                            crossOffset = whiteSpace * 0.5f;
                            break;
                        case alignment_y::bottom:
                            crossOffset = whiteSpace;
                            break;
                        default:
                            break;
                        }
                    }
                    else
                    {
                        switch (desc.alignment.x)
                        {
                        case alignment_x::center:
                            crossOffset = whiteSpace * 0.5f;
                            break;
                        case alignment_x::right:
                            crossOffset = whiteSpace;
                            break;
                        default:
                            break;
                        }
                    }

                    vec2 childOriginForChild = childOrigin;

                    if (isHorizontal)
                    {
                        childOriginForChild.y += crossOffset;
                    }
                    else
                    {
                        childOriginForChild.x += crossOffset;
                    }

                    resolve_element(state, child, childOriginForChild, innerSize, cursor, desc.direction);

                    cursor += childMain + desc.childGap;
                }
            }
        }

        void interpolate(const animated_values& initial,
            const animated_values& target,
            f32 u,
            animation_properties active,
            animated_values& out)
        {
            const auto lerpF = [](f32 a, f32 b, f32 u) OBLO_FORCEINLINE_LAMBDA { return a + (b - a) * u; };

            if (active.contains(animation_property::x))
            {
                out.boundingBox.x = lerpF(initial.boundingBox.x, target.boundingBox.x, u);
            }

            if (active.contains(animation_property::y))
            {
                out.boundingBox.y = lerpF(initial.boundingBox.y, target.boundingBox.y, u);
            }

            if (active.contains(animation_property::width))
            {
                out.boundingBox.width = lerpF(initial.boundingBox.width, target.boundingBox.width, u);
            }

            if (active.contains(animation_property::height))
            {
                out.boundingBox.height = lerpF(initial.boundingBox.height, target.boundingBox.height, u);
            }

            if (active.contains(animation_property::background_color))
            {
                out.backgroundColor = {
                    lerpF(initial.backgroundColor.r, target.backgroundColor.r, u),
                    lerpF(initial.backgroundColor.g, target.backgroundColor.g, u),
                    lerpF(initial.backgroundColor.b, target.backgroundColor.b, u),
                    lerpF(initial.backgroundColor.a, target.backgroundColor.a, u),
                };
            }

            if (active.contains(animation_property::overlay_color))
            {
                out.overlayColor = {
                    lerpF(initial.overlayColor.r, target.overlayColor.r, u),
                    lerpF(initial.overlayColor.g, target.overlayColor.g, u),
                    lerpF(initial.overlayColor.b, target.overlayColor.b, u),
                    lerpF(initial.overlayColor.a, target.overlayColor.a, u),
                };
            }

            if (active.contains(animation_property::corner_radius))
            {
                out.cornerRadius = {
                    lerpF(initial.cornerRadius.x, target.cornerRadius.x, u),
                    lerpF(initial.cornerRadius.y, target.cornerRadius.y, u),
                    lerpF(initial.cornerRadius.z, target.cornerRadius.z, u),
                    lerpF(initial.cornerRadius.w, target.cornerRadius.w, u),
                };
            }
        }

        void finalize_append_child(layout_state& state, u32 parentIndex, u32 index)
        {
            if (parentIndex != invalid_index)
            {
                auto& parent = state.elements[parentIndex];

                if (parent.firstChild == invalid_index)
                {
                    parent.firstChild = index;
                }
                else
                {
                    state.elements[parent.lastChild].nextSibling = index;
                }

                parent.lastChild = index;
            }
        }

        hashed_string_view store_text(bump_allocator& allocator, hashed_string_view text)
        {
            byte* const ptr = allocator.allocate(text.size(), 1u);
            char* const buf = new (ptr) char[text.size()];
            std::memcpy(buf, text.data(), text.size());
            return {string_view{buf, text.size()}, text.hash()};
        }
    }

    f32 ease(easing_function fn, f32 t)
    {
        /// @see https://easings.net/ https://github.com/ai/easings.net
        const f32 x = min(max(t, 0.f), 1.f);

        switch (fn)
        {
        case easing_function::linear:
            return x;
        case easing_function::ease_in:
            return x * x * x;
        case easing_function::ease_out:
            return 1.f - std::pow(1.f - x, 3.f);
        case easing_function::ease_in_out:
            return x < 0.5f ? 4.f * x * x * x : 1.f - std::pow(-2.f * x + 2.f, 3.f) / 2.f;
        case easing_function::ease_in_back:
            return 2.70158f * x * x * x - 1.70158f * x * x;
        case easing_function::ease_out_back:
            return 1.f + 2.70158f * std::pow(x - 1.f, 3.f) + 1.70158f * std::pow(x - 1.f, 2.f);
        case easing_function::ease_out_elastic:
            if (x == 0.f)
            {
                return 0.f;
            }
            else if (x == 1.f)
            {
                return 1.f;
            }
            else
            {
                constexpr f32 c4 = 2.f * pi / 3.f;
                return std::pow(2.f, -10.f * x) * std::sin((x * 10.f - 0.75f) * c4) + 1.f;
            }
        case easing_function::ease_out_bounce:
            return ease_out_bounce(x);
        default:
            return x;
        }
    }

    layout_state* create_state()
    {
        allocator* const allocator = get_global_allocator();

        auto* const memory = allocator->allocate(sizeof(layout_state), alignof(layout_state));

        if (!memory)
        {
            return nullptr;
        }

        layout_state* const state = new (memory) layout_state{};

        if (!state->fonts.init())
        {
            destroy_state(state);
            return nullptr;
        }

        return state;
    }

    void destroy_state(layout_state* state)
    {
        if (state)
        {
            state->fonts.shutdown();

            state->~layout_state();

            allocator* const allocator = get_global_allocator();
            allocator->deallocate(reinterpret_cast<byte*>(state), sizeof(layout_state), alignof(layout_state));
        }
    }

    expected<font_id> load_font_from_file(layout_state& state, cstring_view path)
    {
        return state.fonts.load_font_from_file(path);
    }

    expected<font_id> load_font_from_memory(layout_state& state, span<const byte> data)
    {
        return state.fonts.load_font_from_memory(data);
    }

    void begin_container(layout_state& state, const container_descriptor& desc)
    {
        auto& elements = state.elements;

        const u32 parentIndex =
            state.openContainerIdxStack.empty() ? invalid_index : state.openContainerIdxStack.back();

        const u32 index = elements.size32();

        auto& element = elements.push_back_default();

        element.kind = layout_element_kind::container;
        element.elementId = desc.elementId;
        element.parentIndex = parentIndex;

        element.width = desc.width;
        element.height = desc.height;

        element.data.container = {
            .direction = desc.direction,
            .alignment = desc.alignment,
            .backgroundColor = desc.backgroundColor,
            .cornerRadius = desc.cornerRadius,
            .childGap = desc.childGap,
            .padding = desc.padding,
            .animation = desc.animation,
        };

        finalize_append_child(state, parentIndex, index);

        state.openContainerIdxStack.push_back(index);
    }

    void end_container(layout_state& state)
    {
        OBLO_ASSERT(!state.openContainerIdxStack.empty());

        if (state.openContainerIdxStack.empty())
        {
            return;
        }

        const u32 index = state.openContainerIdxStack.back();
        state.openContainerIdxStack.pop_back();

        auto& elements = state.elements;
        auto& element = elements[index];

        const container_layout_data& desc = element.data.container;

        // Post-order: the children have already been measured, so the content size can be
        // accumulated. Percentage children contribute 0 here; they are expanded against
        // this element's size later, in resolve_element.
        f32 main = 0.f;
        f32 cross = 0.f;
        u32 childCount = 0;

        for (u32 child = element.firstChild; child != invalid_index; child = elements[child].nextSibling)
        {
            const vec2 childSize = elements[child].targetRect.size();

            if (desc.direction == layout_direction::left_to_right)
            {
                main += childSize.x;
                cross = max(cross, childSize.y);
            }
            else
            {
                main += childSize.y;
                cross = max(cross, childSize.x);
            }

            ++childCount;
        }

        if (childCount > 1)
        {
            main += (childCount - 1) * desc.childGap;
        }

        const f32 mainPadding = desc.direction == layout_direction::left_to_right
            ? desc.padding.left + desc.padding.right
            : desc.padding.top + desc.padding.bottom;
        const f32 crossPadding = desc.direction == layout_direction::left_to_right
            ? desc.padding.top + desc.padding.bottom
            : desc.padding.left + desc.padding.right;

        main += mainPadding;
        cross += crossPadding;

        element.contentSize = desc.direction == layout_direction::left_to_right ? vec2{main, cross} : vec2{cross, main};

        // Resolve the final size for sizing kinds that don't depend on the parent.
        element.targetRect.width = resolve_axis_size(element.width, element.contentSize.x, 0.f);
        element.targetRect.height = resolve_axis_size(element.height, element.contentSize.y, 0.f);
    }

    void add_text(layout_state& state, const text_descriptor& desc)
    {
        auto& elements = state.elements;

        const u32 parentIndex =
            state.openContainerIdxStack.empty() ? invalid_index : state.openContainerIdxStack.back();

        const u32 index = elements.size32();

        auto& element = elements.push_back_default();

        // A text element is a leaf in the layout tree. It carries its own measured size
        // and is linked into the current open container like any other child.
        element.kind = layout_element_kind::text;
        element.elementId = desc.elementId;
        element.parentIndex = parentIndex;

        // Just fit for now, not sure if we need to set size externally
        element.width = fit_size();
        element.height = fit_size();

        const FT_Face face = state.fonts.find_font(desc.font);

        // Not sure if we really need to store the text, we may just need the glyphs
        const hashed_string_view storedText = store_text(state.frameAllocator, desc.text);
        const span<const u32> storedGlyphs = text_to_glyphs(state.frameAllocator, face, storedText);

        element.data.text = {
            .text = storedText,
            .glyphs = storedGlyphs,
            .color = desc.color,
            .font = desc.font,
            .fontSize = desc.fontSize,
        };

        const vec2 measured = measure_text(state.fonts, desc.font, face, desc.fontSize, storedGlyphs);

        element.contentSize = measured;
        element.targetRect = {0.f, 0.f, measured.x, measured.y};

        finalize_append_child(state, parentIndex, index);
    }

    void set_layout_size(layout_state& state, vec2 size)
    {
        state.layoutSize = size;
    }

    void begin_frame(layout_state& state, time dt)
    {
        state.elements.clear();
        state.openContainerIdxStack.clear();

        state.animations.begin_frame(dt);

        state.frameAllocator.reset();
    }

    void end_frame(layout_state& state)
    {
        // Resolve every root. The root's parent is the layout itself: its size is the
        // available layout area and its origin is the layout origin.
        for (u32 i = 0; i < state.elements.size(); ++i)
        {
            if (state.elements[i].parentIndex == invalid_index)
            {
                resolve_element(state, i, {}, state.layoutSize, 0.f, layout_direction::left_to_right);
            }
        }

        // Snapshot the resolved elements for next frame's input hit-testing. Bake the
        // rendered (possibly animated) rect and drop the animated pointer so the copy is
        // self-contained.
        state.previousElements.clear();
        state.previousElementIndex.clear();

        for (u32 i = 0; i < state.elements.size(); ++i)
        {
            const auto& e = state.elements[i];

            layout_element snapshot = e;

            if (snapshot.animated)
            {
                snapshot.targetRect = snapshot.animated->boundingBox;
            }

            snapshot.animated = nullptr;

            if (e.elementId != layout_id{})
            {
                state.previousElementIndex.emplace(e.elementId, i);
            }

            state.previousElements.push_back(snapshot);
        }

        state.animations.end_frame();
    }

    const layout_element* find_element(const layout_state& state, layout_id element)
    {
        if (element == layout_id{})
        {
            return nullptr;
        }

        for (const auto& e : state.elements)
        {
            if (e.elementId == element)
            {
                return &e;
            }
        }

        return nullptr;
    }

    const animated_values* update_element(layout_state& state,
        layout_id element,
        layout_id parent,
        vec2 parentOrigin,
        const animated_values& target,
        const animation_config& config)
    {
        if (element == layout_id{})
        {
            return nullptr;
        }

        return state.animations.update(element, parent, parentOrigin, target, config);
    }

    const animated_values* get_animated(const layout_state& state, layout_id element)
    {
        return state.animations.try_get(element);
    }

    span<const layout_element> get_elements(const layout_state& state)
    {
        return state.elements;
    }

    layout_id hit_test(const layout_state& state, vec2 point)
    {
        // Later elements are drawn on top, so scan in reverse to report the topmost hit.
        for (u32 i = u32(state.previousElements.size()); i-- > 0;)
        {
            const auto& e = state.previousElements[i];

            if (!e.elementId)
            {
                continue;
            }

            if (e.get_current_rect().contains(point))
            {
                return e.elementId;
            }
        }

        return layout_id{};
    }

    const rect* get_rect(const layout_state& state, layout_id id)
    {
        if (id == layout_id{})
        {
            return nullptr;
        }

        const auto it = state.previousElementIndex.find(id);

        if (it == state.previousElementIndex.end())
        {
            return nullptr;
        }

        return &state.previousElements[it->second].targetRect;
    }

    animation_record* animation_store::find_record(layout_id element) noexcept
    {
        for (auto& record : m_records)
        {
            if (record.elementId == element)
            {
                return &record;
            }
        }

        return nullptr;
    }

    const animation_record* animation_store::find_record(layout_id element) const noexcept
    {
        for (const auto& record : m_records)
        {
            if (record.elementId == element)
            {
                return &record;
            }
        }

        return nullptr;
    }

    void animation_store::begin_frame(time dt)
    {
        m_dt = dt;

        for (auto& record : m_records)
        {
            record.appearedThisFrame = false;
        }
    }

    void animation_store::snap_to_target(animation_record& record)
    {
        record.state = animation_state::idle;
        record.elapsedTime = time{};
        record.activeProperties = {};
        record.initial = record.target;
        record.current = record.target;
    }

    void animation_store::advance(animation_record& record, time dt)
    {
        if (record.duration <= time{})
        {
            snap_to_target(record);
            return;
        }

        // The elapsed time is used *before* adding this frame's dt, so the first frame of a
        // animation renders the initial state.
        const f32 t = min(to_f32_seconds(record.elapsedTime) / to_f32_seconds(record.duration), 1.f);
        const f32 u = ease(record.easing, t);

        interpolate(record.initial, record.target, u, record.activeProperties, record.current);

        record.elapsedTime.hns += dt.hns;

        if (t >= 1.f)
        {
            record.state = animation_state::idle;
            record.elapsedTime = time{};
            record.activeProperties = {};
            record.current = record.target;
        }
    }

    void animation_store::start_exit(animation_record& record)
    {
        OBLO_ASSERT(record.exitFinal);

        record.state = animation_state::exiting;
        record.initial = record.current;
        record.target = record.exitFinal(record.initial, record.properties);
        record.elapsedTime = time{};
        record.activeProperties = record.properties;
    }

    const animated_values* animation_store::update(layout_id element,
        layout_id parent,
        vec2 parentOrigin,
        const animated_values& target,
        const animation_config& config)
    {
        const vec2 newRelativePosition = target.boundingBox.position() - parentOrigin;

        auto* const record = find_record(element);

        if (!record)
        {
            auto& r = m_records.emplace_back();

            r.elementId = element;
            r.parentId = parent;
            r.oldRelativePosition = newRelativePosition;
            r.appearedThisFrame = true;
            r.declaredThisFrame = true;
            r.properties = config.properties;
            r.easing = config.easing;
            r.duration = config.duration;
            r.exitFinal = config.exit.setFinalState;
            r.animationOut = r.exitFinal != nullptr;
            r.target = target;

            const auto* const parentRecord = parent ? find_record(parent) : nullptr;
            const bool parentAppeared = parentRecord != nullptr && parentRecord->appearedThisFrame;
            const bool animateEnter =
                config.enter.setInitialState != nullptr && (config.enter.triggerOnFirstParentFrame || !parentAppeared);

            if (animateEnter && r.duration > time::from_seconds(0.f))
            {
                r.state = animation_state::entering;
                r.initial = config.enter.setInitialState(target, config.properties);
                r.current = r.initial;
                r.activeProperties = config.properties;
                return &r.current;
            }

            snap_to_target(r);
            return &r.current;
        }

        record->declaredThisFrame = true;

        if (record->state == animation_state::exiting)
        {
            // The element reappeared while it was animating out; snap to the new target and
            // let the change detection below animate towards it.
            record->state = animation_state::idle;
            record->current = record->target;
            record->activeProperties = {};
        }

        record->reparented = record->parentId != parent;
        record->parentId = parent;

        const auto oldTarget = record->target;
        const auto& props = config.properties;

        animation_properties newActive{};

        if (props.contains(animation_property::x) && !float_equal(oldTarget.boundingBox.x, target.boundingBox.x) &&
            (record->reparented || !float_equal(record->oldRelativePosition.x, newRelativePosition.x)))
        {
            newActive.set(animation_property::x);
        }

        if (props.contains(animation_property::y) && !float_equal(oldTarget.boundingBox.y, target.boundingBox.y) &&
            (record->reparented || !float_equal(record->oldRelativePosition.y, newRelativePosition.y)))
        {
            newActive.set(animation_property::y);
        }

        if (props.contains(animation_property::width) &&
            !float_equal(oldTarget.boundingBox.width, target.boundingBox.width))
        {
            newActive.set(animation_property::width);
        }

        if (props.contains(animation_property::height) &&
            !float_equal(oldTarget.boundingBox.height, target.boundingBox.height))
        {
            newActive.set(animation_property::height);
        }

        if (props.contains(animation_property::background_color) &&
            !float_equal(oldTarget.backgroundColor, target.backgroundColor))
        {
            newActive.set(animation_property::background_color);
        }

        if (props.contains(animation_property::overlay_color) &&
            !float_equal(oldTarget.overlayColor, target.overlayColor))
        {
            newActive.set(animation_property::overlay_color);
        }

        if (props.contains(animation_property::corner_radius) &&
            !float_equal(oldTarget.cornerRadius, target.cornerRadius))
        {
            newActive.set(animation_property::corner_radius);
        }

        record->oldRelativePosition = newRelativePosition;
        record->target = target;

        if (record->state == animation_state::idle)
        {
            if (newActive.is_empty())
            {
                record->initial = target;
                record->current = target;
                record->activeProperties = {};
                return &record->current;
            }

            // Start a animation from the last rendered state.
            record->state = animation_state::animationing;
            record->initial = record->current;
            record->activeProperties = newActive;
            record->elapsedTime = {};
        }
        else
        {
            // Already animating; re-target from the current interpolated values.
            if (!newActive.is_empty())
            {
                record->initial = record->current;
                record->elapsedTime = {};
                record->activeProperties |= newActive;
            }
        }

        advance(*record, m_dt);

        return &record->current;
    }

    void animation_store::end_frame()
    {
        for (usize i = 0; i < m_records.size();)
        {
            auto& record = m_records[i];

            if (record.declaredThisFrame)
            {
                record.declaredThisFrame = false;
                ++i;
                continue;
            }

            if (record.state == animation_state::exiting)
            {
                if (record.duration <= time{})
                {
                    m_records.erase_unordered(m_records.begin() + i);
                    continue;
                }

                const f32 t = min(to_f32_seconds(record.elapsedTime) / to_f32_seconds(record.duration), 1.f);
                const f32 u = ease(record.easing, t);

                interpolate(record.initial, record.target, u, record.activeProperties, record.current);

                record.elapsedTime.hns += m_dt.hns;

                if (t >= 1.f)
                {
                    m_records.erase_unordered(m_records.begin() + i);
                    continue;
                }

                ++i;
                continue;
            }

            if (record.animationOut)
            {
                start_exit(record);
                ++i;
                continue;
            }

            m_records.erase_unordered(m_records.begin() + i);
        }
    }

    const animated_values* animation_store::try_get(layout_id element) const
    {
        const auto* const record = find_record(element);
        return record ? &record->current : nullptr;
    }

    animated_values* animation_store::try_get(layout_id element)
    {
        auto* const record = find_record(element);
        return record ? &record->current : nullptr;
    }

    span<const animation_record> animation_store::records() const
    {
        return m_records;
    }

    span<animation_record> animation_store::records()
    {
        return m_records;
    }

    bool animation_store::empty() const noexcept
    {
        return m_records.empty();
    }

    usize animation_store::size() const noexcept
    {
        return m_records.size();
    }

    void animation_store::clear() noexcept
    {
        m_records.clear();
    }
}
