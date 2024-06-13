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
        explicit gizmo_handler(u32 id) : m_id{id} {}

        bool handle_translation(ecs::entity_registry& reg,
            std::span<const ecs::entity> entities,
            vec2 origin,
            vec2 size,
            ecs::entity cameraEntity);

    private:
        u32 m_id{};
    };
}