#include <oblo/ui/layout.hpp>
#include <oblo/ui/layout_impl.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/constants.hpp>
#include <oblo/math/float.hpp>

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

        void resolve_element(layout_state& state,
            u32 index,
            vec2 parentOrigin,
            vec2 parentInnerSize,
            f32 mainCursor,
            layout_direction parentDirection)
        {
            auto& elements = state.elements;
            auto& element = elements[index];
            const auto& desc = element.desc;

            // Percentage sizing is resolved against the parent's inner size (the parent's
            // padding has already been removed).
            const vec2 size{
                resolve_axis_size(desc.width, element.contentSize.x, parentInnerSize.x),
                resolve_axis_size(desc.height, element.contentSize.y, parentInnerSize.y),
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

            // Feed the transition system, parents before children.
            if (element.elementId != layout_id{} && desc.hasTransition)
            {
                animated_values target;
                target.boundingBox = element.targetRect;
                target.cornerRadius = element.cornerRadius;

                const layout_id parentId =
                    element.parentIndex != invalid_index ? elements[element.parentIndex].elementId : layout_id{};

                element.animated = update_element(state,
                    element.elementId,
                    parentId,
                    element.targetRect.position(),
                    target,
                    desc.transition);
            }

            // Position the children along this element's layout axis, inset by this
            // element's padding. The padding offsets the children, not the element itself.
            const vec2 inner_size = {max(size.x - desc.padding.left - desc.padding.right, 0.f),
                max(size.y - desc.padding.top - desc.padding.bottom, 0.f)};

            const vec2 childOrigin = element.targetRect.position() + vec2{desc.padding.left, desc.padding.top};

            f32 cursor = 0.f;

            for (u32 child = element.firstChild; child != invalid_index; child = elements[child].nextSibling)
            {
                resolve_element(state, child, childOrigin, inner_size, cursor, desc.direction);

                const vec2 childSize = elements[child].targetRect.size();
                cursor +=
                    (desc.direction == layout_direction::left_to_right ? childSize.x : childSize.y) + desc.child_gap;
            }
        }

        void interpolate(const animated_values& initial,
            const animated_values& target,
            f32 u,
            animation_properties active,
            animated_values& out)
        {
            const auto lerpF = [](f32 a, f32 b, f32 u) { return a + (b - a) * u; };

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

        return new (memory) layout_state{};
    }

    void destroy_state(layout_state* state)
    {
        if (state)
        {
            state->~layout_state();

            allocator* const allocator = get_global_allocator();
            allocator->deallocate(reinterpret_cast<byte*>(state), sizeof(layout_state), alignof(layout_state));
        }
    }

    void begin_container(layout_state& state, const container_descriptor& desc)
    {
        auto& elements = state.elements;

        const u32 parentIndex =
            state.openContainerIdxStack.empty() ? invalid_index : state.openContainerIdxStack.back();

        const u32 index = u32(elements.size());

        auto& element = elements.push_back_default();

        element.desc = desc;
        element.elementId = desc.elementId;
        element.parentIndex = parentIndex;
        element.cornerRadius = desc.cornerRadius;

        if (parentIndex != invalid_index)
        {
            auto& parent = elements[parentIndex];

            if (parent.firstChild == invalid_index)
            {
                parent.firstChild = index;
            }
            else
            {
                elements[parent.lastChild].nextSibling = index;
            }

            parent.lastChild = index;
        }

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
        const auto& desc = element.desc;

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
            main += (childCount - 1) * desc.child_gap;
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
        element.targetRect.width = resolve_axis_size(desc.width, element.contentSize.x, 0.f);
        element.targetRect.height = resolve_axis_size(desc.height, element.contentSize.y, 0.f);
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
        const transition_config& config)
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

    std::span<const layout_element> get_elements(const layout_state& state)
    {
        return state.elements;
    }

    transition_record* transition_store::find_record(layout_id element) noexcept
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

    const transition_record* transition_store::find_record(layout_id element) const noexcept
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

    void transition_store::begin_frame(time dt)
    {
        m_dt = dt;

        for (auto& record : m_records)
        {
            record.appearedThisFrame = false;
        }
    }

    void transition_store::snap_to_target(transition_record& record)
    {
        record.state = transition_state::idle;
        record.elapsedTime = time{};
        record.activeProperties = {};
        record.initial = record.target;
        record.current = record.target;
    }

    void transition_store::advance(transition_record& record, time dt)
    {
        if (record.duration <= time{})
        {
            snap_to_target(record);
            return;
        }

        // The elapsed time is used *before* adding this frame's dt, so the first frame of a
        // transition renders the initial state.
        const f32 t = min(to_f32_seconds(record.elapsedTime) / to_f32_seconds(record.duration), 1.f);
        const f32 u = ease(record.easing, t);

        interpolate(record.initial, record.target, u, record.activeProperties, record.current);

        record.elapsedTime.hns += dt.hns;

        if (t >= 1.f)
        {
            record.state = transition_state::idle;
            record.elapsedTime = time{};
            record.activeProperties = {};
            record.current = record.target;
        }
    }

    void transition_store::start_exit(transition_record& record)
    {
        OBLO_ASSERT(record.exitFinal);

        record.state = transition_state::exiting;
        record.initial = record.current;
        record.target = record.exitFinal(record.initial, record.properties);
        record.elapsedTime = time{};
        record.activeProperties = record.properties;
    }

    const animated_values* transition_store::update(layout_id element,
        layout_id parent,
        vec2 parentOrigin,
        const animated_values& target,
        const transition_config& config)
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
            r.transitionOut = r.exitFinal != nullptr;
            r.target = target;

            const auto* const parentRecord = parent ? find_record(parent) : nullptr;
            const bool parentAppeared = parentRecord != nullptr && parentRecord->appearedThisFrame;
            const bool animateEnter =
                config.enter.setInitialState != nullptr && (config.enter.triggerOnFirstParentFrame || !parentAppeared);

            if (animateEnter && r.duration > time::from_seconds(0.f))
            {
                r.state = transition_state::entering;
                r.initial = config.enter.setInitialState(target, config.properties);
                r.current = r.initial;
                r.activeProperties = config.properties;
                return &r.current;
            }

            snap_to_target(r);
            return &r.current;
        }

        record->declaredThisFrame = true;

        if (record->state == transition_state::exiting)
        {
            // The element reappeared while it was animating out; snap to the new target and
            // let the change detection below animate towards it.
            record->state = transition_state::idle;
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

        if (record->state == transition_state::idle)
        {
            if (newActive.is_empty())
            {
                record->initial = target;
                record->current = target;
                record->activeProperties = {};
                return &record->current;
            }

            // Start a transition from the last rendered state.
            record->state = transition_state::transitioning;
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

    void transition_store::end_frame()
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

            if (record.state == transition_state::exiting)
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

            if (record.transitionOut)
            {
                start_exit(record);
                ++i;
                continue;
            }

            m_records.erase_unordered(m_records.begin() + i);
        }
    }

    const animated_values* transition_store::try_get(layout_id element) const
    {
        const auto* const record = find_record(element);
        return record ? &record->current : nullptr;
    }

    animated_values* transition_store::try_get(layout_id element)
    {
        auto* const record = find_record(element);
        return record ? &record->current : nullptr;
    }

    std::span<const transition_record> transition_store::records() const
    {
        return m_records;
    }

    std::span<transition_record> transition_store::records()
    {
        return m_records;
    }

    bool transition_store::empty() const noexcept
    {
        return m_records.empty();
    }

    usize transition_store::size() const noexcept
    {
        return m_records.size();
    }

    void transition_store::clear() noexcept
    {
        m_records.clear();
    }
}
