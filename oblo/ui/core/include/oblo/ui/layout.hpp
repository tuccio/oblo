#pragma once

#include <oblo/ui/forward.hpp>

namespace oblo::ui::layout
{
    enum class direction : u8
    {
        left_to_right,
        top_to_bottom,
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

    struct percentage_sizing
    {
        f32 size;
    };

    struct sizing
    {
        sizing_kind kind;

        union {
            fit_sizing fit;
            fixed_sizing fixed;
            percentage_sizing percentage;
        } sizing;
    };

    struct container_descriptor
    {
        direction direction;
        sizing width;
        sizing height;
    };

    layout_state* create_state();
    void destroy_state(layout_state* state);

    void begin_container(layout_state& state, const container_descriptor& desc);
    void end_container();

    class container_scope
    {
    public:
        container_scope(const container_scope&) = delete;
        container_scope(container_scope&&) noexcept = delete;

        container_scope& operator=(const container_scope&) = delete;
        container_scope& operator=(container_scope&&) noexcept = delete;

        ~container_scope()
        {
            end_container();
        }

    private:
        container_scope() = default;

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

        container_builder&& direction(direction dir) &&
        {
            m_desc.direction = dir;
            return static_cast<container_builder&&>(*this);
        }

        container_builder&& width(const fixed_sizing& s) &&
        {
            m_desc.width = {
                .kind = sizing_kind::fixed,
                .sizing = {.fixed = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_builder&& height(const fixed_sizing& s) &&
        {
            m_desc.height = {
                .kind = sizing_kind::fixed,
                .sizing = {.fixed = s},
            };

            return static_cast<container_builder&&>(*this);
        }

        container_scope build(layout_state& state) &&
        {
            begin_container(state, m_desc);
            return container_scope{};
        }

    private:
        container_descriptor m_desc{};
    };
}