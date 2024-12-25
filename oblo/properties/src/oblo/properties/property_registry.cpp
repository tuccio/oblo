#include <oblo/properties/property_registry.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_tree.hpp>
#include <oblo/reflection/concepts/random_access_container.hpp>
#include <oblo/reflection/reflection_registry.hpp>

#include <algorithm>

namespace oblo
{
    struct uuid;

    namespace
    {
        property* try_add_property_impl(const reflection::reflection_registry* reflection,
            std::unordered_map<type_id, property_kind> kindLookups,
            property_tree& tree,
            u32 currentNodeIndex,
            const type_id& type,
            cstring_view name,
            u32 offset)
        {
            type_id lookupType = type;
            bool isEnum = false;

            if (const auto e = reflection->find_enum(type))
            {
                lookupType = reflection->get_underlying_type(e);
                isEnum = true;
            }

            if (const auto it = kindLookups.find(lookupType); it != kindLookups.end())
            {
                auto& p = tree.properties.push_back({
                    .type = type,
                    .name = name,
                    .kind = it->second,
                    .isEnum = isEnum,
                    .offset = offset,
                    .parent = currentNodeIndex,
                });

                return &p;
            }

            return nullptr;
        }

        void add_attributes(property_tree& tree, property& prop, std::span<const reflection::attribute_data> attributes)
        {
            const u32 firstAttribute = u32(tree.attributes.size());

            for (const auto& attribute : attributes)
            {
                tree.attributes.emplace_back(attribute.type, attribute.ptr);
            }

            const u32 lastAttribute = u32(tree.attributes.size());

            prop.firstAttribute = firstAttribute;
            prop.lastAttribute = lastAttribute;
        }
    }

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

        m_kindLookups.emplace(get_type_id<uuid>(), property_kind::uuid);
        m_kindLookups.emplace(get_type_id<string>(), property_kind::string);
    }

    property_kind property_registry::find_property_kind(const type_id& type) const
    {
        const auto it = m_kindLookups.find(type);

        if (it == m_kindLookups.end())
        {
            return property_kind::enum_max;
        }

        return it->second;
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

                const u32 firstAttribute = u32(tree.attributes.size());

                for (const auto& attribute : field.attributes)
                {
                    tree.attributes.emplace_back(attribute.type, attribute.ptr);
                }

                const u32 lastAttribute = u32(tree.attributes.size());

                tree.nodes.push_back({
                    .type = field.type,
                    .name = field.name,
                    .offset = field.offset,
                    .parent = currentNodeIndex,
                    .firstAttribute = firstAttribute,
                    .lastAttribute = lastAttribute,
                });

                const auto rac = m_reflection->find_concept<reflection::random_access_container>(fieldType);

                if (rac)
                {
                    try_add_property_impl(m_reflection,
                        m_kindLookups,
                        tree,
                        newNodeIndex,
                        get_type_id<usize>(),
                        notable_properties::array_size,
                        0);

                    auto& n = tree.nodes.back();
                    n.isArray = true;
                    n.arrayId = u32(tree.arrays.size());

                    tree.arrays.push_back({
                        .size = rac->size,
                        .at = rac->at,
                    });

                    const auto valueType = m_reflection->find_type(rac->valueType);

                    // If the value type is an enum or a property we can add the $element property, otherwise add
                    // another child and visit that
                    auto parentNode = newNodeIndex;

                    if (!m_kindLookups.contains(rac->valueType) && !m_reflection->try_get_enum(valueType))
                    {
                        parentNode = u32(tree.nodes.size());

                        tree.nodes.back().firstChild = parentNode;

                        tree.nodes.push_back({
                            .type = rac->valueType,
                            .parent = newNodeIndex,
                        });

                        build_recursive(tree, parentNode, valueType);
                    }
                    else
                    {
                        try_add_property_impl(m_reflection,
                            m_kindLookups,
                            tree,
                            newNodeIndex,
                            rac->valueType,
                            notable_properties::array_element,
                            0);
                    }
                }
                else
                {
                    build_recursive(tree, newNodeIndex, fieldType);
                }

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
        auto* p = try_add_property_impl(m_reflection,
            m_kindLookups,
            tree,
            currentNodeIndex,
            field.type,
            field.name,
            field.offset);

        if (p)
        {
            add_attributes(tree, *p, field.attributes);
        }

        return p != nullptr;
    }
}