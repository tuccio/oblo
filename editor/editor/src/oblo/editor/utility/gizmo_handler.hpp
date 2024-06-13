#pragma once

#include <oblo/ecs/handles.hpp>
#include <oblo/math/vec2.hpp>

#include <span>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor::gizmo_handler
{
    bool handle_transform_gizmos(ecs::entity_registry& reg,
        std::span<const ecs::entity> entities,
        vec2 origin,
        vec2 size,
        const ecs::entity cameraEntity);
}