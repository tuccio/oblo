#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/ecs/handles.hpp>

namespace oblo::ecs
{
    class entity_registry;
}

namespace oblo::editor
{
    class viewport final
    {
    public:
        viewport() = delete;
        viewport(ecs::entity_registry& entities);
        viewport(const viewport&) = delete;
        viewport(viewport&&) noexcept = delete;
        ~viewport();

        viewport& operator=(const viewport&) = delete;
        viewport& operator=(viewport&&) noexcept = delete;

        bool update();

    private:
        ecs::entity_registry* m_entities;
        ecs::entity m_entity{};
    };
}