#pragma once

#include <oblo/core/unique_ptr.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/math/vec2.hpp>

#include <span>

namespace oblo
{
    class resource_registry;
    struct viewport_component;
}

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    class gizmo_handler
    {
    public:
        enum class operation : u8
        {
            translation,
            rotation,
            scale
        };

    public:
        gizmo_handler();
        gizmo_handler(const gizmo_handler&) = delete;
        gizmo_handler(gizmo_handler&&) noexcept = delete;
        ~gizmo_handler();

        gizmo_handler& operator=(const gizmo_handler&) = delete;
        gizmo_handler& operator=(gizmo_handler&&) noexcept = delete;

        void init();
        void set_id(u32 id);

        operation get_operation() const;
        void set_operation(operation op);

        bool handle(const resource_registry& resources,
            ecs::entity_registry& reg,
            std::span<const ecs::entity> entities,
            vec2 origin,
            vec2 size,
            ecs::entity cameraEntity,
            viewport_component* viewport);

    private:
        struct impl;
        unique_ptr<impl> m_impl;
    };
}
