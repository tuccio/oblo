#pragma once

#include <oblo/ecs/handles.hpp>
#include <oblo/math/vec2.hpp>

#include <span>

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
        explicit gizmo_handler() = default;
        explicit gizmo_handler(u32 id) : m_id{id} {}

        void set_id(u32 id);

        operation get_operation() const;
        void set_operation(operation op);

        bool handle(ecs::entity_registry& reg,
            std::span<const ecs::entity> entities,
            vec2 origin,
            vec2 size,
            ecs::entity cameraEntity);

    private:
        u32 m_id{};
        operation m_op{};
    };
}