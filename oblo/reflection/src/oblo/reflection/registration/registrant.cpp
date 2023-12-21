#include <oblo/reflection/registration/registrant.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/reflection/reflection_registry_impl.hpp>

namespace oblo::reflection
{
    u32 reflection_registry::registrant::add_new_type(const type_id& type, u32 size, u32 alignment, type_kind kind)
    {
        const auto [it, ok] = m_impl.typesMap.emplace(type, ecs::entity{});
        OBLO_ASSERT(ok);

        if (!ok)
        {
            return 0;
        }

        ecs::entity e{};

        switch (kind)
        {
        case type_kind::fundamental_kind:
            e = m_impl.registry.create<type_data, fundamental_tag>();
            break;

        case type_kind::class_kind:
            e = m_impl.registry.create<type_data, class_data>();
            break;

        default:
            unreachable();
        }
        it->second = e;

        auto& typeData = m_impl.registry.get<type_data>(e);

        typeData = {
            .type = type,
            .kind = kind,
            .size = size,
            .alignment = alignment,
        };

        return e.value;
    }

    void reflection_registry::registrant::add_field(
        u32 entityIndex, const type_id& type, std::string_view name, u32 offset)
    {
        const ecs::entity e{entityIndex};

        auto& classData = m_impl.registry.get<class_data>(e);

        classData.fields.emplace_back(field_data{
            .type = type,
            .name = name,
            .offset = offset,
        });
    }

    void reflection_registry::registrant::add_tag(u32 entityIndex, const type_id& type)
    {
        const ecs::entity e{entityIndex};
        const auto tag = m_impl.typesRegistry.get_or_register_tag({.type = type});

        ecs::component_and_tag_sets sets{};
        sets.tags.add(tag);

        m_impl.registry.add(e, sets);
    }

    void reflection_registry::registrant::add_concept(
        u32 entityIndex, const type_id& type, u32 size, u32 alignment, const ranged_type_erasure& rte, void* src)
    {
        const ecs::entity e{entityIndex};

        const auto component = m_impl.typesRegistry.get_or_register_component({
            .type = type,
            .size = size,
            .alignment = alignment,
            .create = rte.create,
            .destroy = rte.destroy,
            .move = rte.move,
            .moveAssign = rte.moveAssign,
        });

        ecs::component_and_tag_sets sets{};
        sets.components.add(component);

        m_impl.registry.add(e, sets);
        auto* const dst = m_impl.registry.try_get(e, component);

        rte.moveAssign(dst, src, 1u);
    }
}