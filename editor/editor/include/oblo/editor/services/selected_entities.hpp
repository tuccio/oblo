#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>
#include <oblo/ecs/handles.hpp>

namespace oblo::editor
{
    class selected_entities
    {
    public:
        void add(ecs::entity e);
        void add(std::span<const ecs::entity> entities);

        void remove(ecs::entity e);

        void clear();

        bool contains(ecs::entity e) const;

        std::span<const ecs::entity> get() const;

    private:
        // TODO: (#8) Should implement a flat_dense_set
        flat_dense_map<ecs::entity, ecs::entity> m_selected;
    };

    inline void selected_entities::add(ecs::entity e)
    {
        m_selected.emplace(e, e);
    }

    inline void selected_entities::add(std::span<const ecs::entity> entities)
    {
        for (const auto e : entities)
        {
            add(e);
        }
    }

    inline void selected_entities::remove(ecs::entity e)
    {
        m_selected.erase(e);
    }

    inline void selected_entities::clear()
    {
        m_selected.clear();
    }

    inline bool selected_entities::contains(ecs::entity e) const
    {
        return m_selected.try_find(e) != nullptr;
    }

    inline std::span<const ecs::entity> selected_entities::get() const
    {
        return m_selected.keys();
    }
}