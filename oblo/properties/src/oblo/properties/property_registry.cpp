#include <oblo/properties/property_registry.hpp>

#include <oblo/core/utility.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <algorithm>

namespace oblo
{
    property_registry::property_registry() = default;

    property_registry::~property_registry() = default;

    void property_registry::init(const reflection::reflection_registry& reflection)
    {
        m_reflection = &reflection;

        m_kindLookups.emplace(get_type_id<bool>(), property_kind::boolean);

        m_kindLookups.emplace(get_type_id<f32>(), property_kind::f32);
        m_kindLookups.emplace(get_type_id<f64>(), property_kind::f64);

        m_kindLookups.emplace(get_type_id<u8>(), property_kind::u8);
        m_kindLookups.emplace(get_type_id<u16>(), property_kind::u16);
        m_kindLookups.emplace(get_type_id<u32>(), property_kind::u32);
        m_kindLookups.emplace(get_type_id<u64>(), property_kind::u64);

        m_kindLookups.emplace(get_type_id<i8>(), property_kind::u8);
        m_kindLookups.emplace(get_type_id<i16>(), property_kind::u16);
        m_kindLookups.emplace(get_type_id<i32>(), property_kind::u32);
        m_kindLookups.emplace(get_type_id<i64>(), property_kind::u64);
    }

    const property_tree* property_registry::build_from_reflection(const type_id& type)
    {
        const auto [it, inserted] = m_propertyTrees.emplace(type, property_tree{});
        auto& tree = it->second;

        if (!inserted)
        {
            return &tree;
        }

        tree.nodes.push_back({
            .type = type,
        });

        const auto typeHandle = m_reflection->find_type(type);

        if (!typeHandle)
        {
            // TODO: log?
            m_propertyTrees.erase(it);
            return nullptr;
        }

        build_recursive(tree, 0, typeHandle);

        std::stable_sort(tree.properties.begin(),
            tree.properties.end(),
            [](const property& lhs, const property& rhs) { return lhs.parent < rhs.parent; });

        u32 currentNodeIndex = 0;
        u32 nextPropertyIndex = 0;

        while (true)
        {
            if (nextPropertyIndex == tree.properties.size())
            {
                break;
            }

            for (const u32 propertyParent = tree.properties[nextPropertyIndex].parent;
                 propertyParent > currentNodeIndex;)
            {
                ++currentNodeIndex;
            }

            if (currentNodeIndex >= tree.nodes.size())
            {
                break;
            }

            auto& node = tree.nodes[currentNodeIndex];
            node.firstProperty = nextPropertyIndex;
            node.lastProperty = nextPropertyIndex;

            while (nextPropertyIndex < tree.properties.size() &&
                currentNodeIndex == tree.properties[nextPropertyIndex].parent)
            {
                node.lastProperty = ++nextPropertyIndex;
            }
        }

        return &tree;
    }

    const property_tree* property_registry::try_get(const type_id& type) const
    {
        const auto it = m_propertyTrees.find(type);
        return it == m_propertyTrees.end() ? nullptr : &it->second;
    }

    void property_registry::build_recursive(property_tree& tree, u32 currentNodeIndex, reflection::type_handle type)
    {
        const auto classHandle = m_reflection->try_get_class(type);

        if (!classHandle)
        {
            // TODO: log?
            return;
        }

        u32 prevSibling{};

        const std::span fields = m_reflection->get_fields(classHandle);

        for (const auto& field : fields)
        {
            const auto fieldType = m_reflection->find_type(field.type);

            if (!try_add_property(tree, currentNodeIndex, field))
            {
                const u32 newNodeIndex = narrow_cast<u32>(tree.nodes.size());

                tree.nodes.push_back({
                    .type = field.type,
                    .name = std::string{field.name},
                    .parent = currentNodeIndex,
                    .offset = field.offset,
                });

                build_recursive(tree, newNodeIndex, fieldType);

                if (prevSibling != 0)
                {
                    tree.nodes[prevSibling].firstSibling = newNodeIndex;
                }
                else
                {
                    tree.nodes[currentNodeIndex].firstChild = newNodeIndex;
                }

                prevSibling = newNodeIndex;
            }
        }
    }

    bool property_registry::try_add_property(
        property_tree& tree, u32 currentNodeIndex, const reflection::field_data& field)
    {
        if (const auto it = m_kindLookups.find(field.type); it != m_kindLookups.end())
        {
            tree.properties.push_back({
                .type = field.type,
                .name = std::string{field.name},
                .kind = it->second,
                .offset = field.offset,
                .parent = currentNodeIndex,
            });

            return true;
        }

        return false;
    }
}