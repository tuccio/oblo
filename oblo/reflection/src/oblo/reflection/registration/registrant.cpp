#include <oblo/reflection/registration/registrant.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/reflection/reflection_registry_impl.hpp>

namespace oblo::reflection
{
    u32 reflection_registry::registrant::add_type(
        const type_id& type, u32 size, u32 alignment, type_kind kind, bool* isNew)
    {
        const auto [it, inserted] = m_impl.typesMap.emplace(type, ecs::entity{});

        if (isNew)
        {
            *isNew = inserted;
        }

        if (!inserted)
        {
            const ecs::entity e = it->second;
            OBLO_ASSERT(e);

            switch (kind)
            {
            case type_kind::fundamental_kind:
                OBLO_ASSERT((m_impl.registry.has<type_data, fundamental_tag>(e)));
                break;

            case type_kind::class_kind:
                OBLO_ASSERT((m_impl.registry.has<type_data, class_data>(e)));
                break;

            case type_kind::enum_kind:
                OBLO_ASSERT((m_impl.registry.has<type_data, enum_data>(e)));
                break;

            case type_kind::array_kind:
                OBLO_ASSERT((m_impl.registry.has<type_data, array_data>(e)));
                break;

            default:
                unreachable();
            }

            return e.value;
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

        case type_kind::enum_kind:
            e = m_impl.registry.create<type_data, enum_data>();
            break;

        case type_kind::array_kind:
            e = m_impl.registry.create<type_data, array_data>();
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

    u32 reflection_registry::registrant::add_enum_type(
        const type_id& type, u32 size, u32 alignment, const type_id& underlying, bool* isNew)
    {
        const auto e = add_type(type, size, alignment, type_kind::enum_kind, isNew);

        if (e != 0)
        {
            auto& enumData = m_impl.registry.get<enum_data>(ecs::entity{e});
            enumData.underlyingType = underlying;
        }

        return e;
    }

    u32 reflection_registry::registrant::add_field(u32 entityIndex, const type_id& type, cstring_view name, u32 offset)
    {
        const ecs::entity e{entityIndex};

        auto& classData = m_impl.registry.get<class_data>(e);

        const u32 fieldIndex = u32(classData.fields.size());

        classData.fields.emplace_back(field_data{
            .type = type,
            .name = name,
            .offset = offset,
        });

        return fieldIndex;
    }

    void* reflection_registry::registrant::add_field_attribute(
        u32 entityIndex, u32 fieldIndex, const type_id& type, u32 size, u32 alignment, void (*destroy)(void*))
    {
        const ecs::entity e{entityIndex};

        auto& classData = m_impl.registry.get<class_data>(e);

        auto& any = classData.attributeStorage.emplace_back(&m_impl.pool,
            m_impl.pool.allocate(size, alignment),
            destroy,
            size,
            alignment);

        classData.fields[fieldIndex].attributes.emplace_back(type, any.get());
        return any.get();
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

    void reflection_registry::registrant::add_enumerator(
        u32 entityIndex, cstring_view name, std::span<const byte> value)
    {
        const ecs::entity e{entityIndex};

        auto& enumData = m_impl.registry.get<enum_data>(e);
        OBLO_ASSERT(value.size() == m_impl.registry.get<type_data>(e).size);

        enumData.names.push_back(name);
        enumData.values.append(value.begin(), value.end());
    }

    void reflection_registry::registrant::make_array_type(u32 entityIndex, std::span<const usize> extents)
    {
        const ecs::entity e{entityIndex};
        auto& arrayData = m_impl.registry.get<array_data>(e);
        arrayData.extents.assign(extents.begin(), extents.end());
    }
}