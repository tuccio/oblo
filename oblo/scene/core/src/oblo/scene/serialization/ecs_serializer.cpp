#include <oblo/scene/serialization/ecs_serializer.hpp>

#include <oblo/core/overload.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/visit.hpp>
#include <oblo/scene/components/children_component.hpp>
#include <oblo/scene/components/parent_component.hpp>

namespace oblo::ecs_serializer
{
    namespace
    {
        namespace json_strings
        {
            constexpr hashed_string_view entitiesArray = "entities";
            constexpr hashed_string_view entityChildren = "children";
            constexpr hashed_string_view componentsObject = "components";
        }

        hashed_string_view sanitize_name(string_view name)
        {
            return name.starts_with(meta_properties::prefix) ? hashed_string_view{} : hashed_string_view{name};
        }
    }

    expected<> write(data_document& doc,
        u32 docRoot,
        const ecs::entity_registry& reg,
        const property_registry& propertyRegistry,
        const write_config& cfg)
    {
        const u32 entitiesArray = doc.child_array(docRoot, json_strings::entitiesArray);

        const auto& typeRegistry = reg.get_type_registry();

        dynamic_array<u32> componentOffsets;
        componentOffsets.reserve(64);

        dynamic_array<byte*> componentArrays;
        componentArrays.reserve(64);

        deque<u32> nodeStack;
        deque<const byte*> ptrStack;

        struct entity_stack_entry
        {
            ecs::entity id;
            u32 parentEntityObject;
        };

        const auto rootsRange = reg.range<>().exclude<parent_component>();

        deque<entity_stack_entry> stack;

        auto skipTypes = cfg.skipTypes;
        skipTypes.components.add(typeRegistry.find_component<parent_component>());
        skipTypes.components.add(typeRegistry.find_component<children_component>());

        for (const auto& chunk : rootsRange)
        {
            for (ecs::entity root : chunk.get<ecs::entity>())
            {
                stack.assign(1,
                    {
                        .id = root,
                        .parentEntityObject = entitiesArray,
                    });

                while (!stack.empty())
                {
                    const auto [entityId, parentEntityObject] = stack.back();
                    stack.pop_back();

                    const auto componentsAndTags = reg.get_component_and_tag_sets(entityId);

                    if (!componentsAndTags.components.intersection(cfg.skipEntities.components).is_empty() ||
                        !componentsAndTags.tags.intersection(cfg.skipEntities.tags).is_empty())
                    {
                        continue;
                    }

                    const std::span componentTypes = reg.get_component_types(entityId);

                    componentOffsets.assign_default(componentTypes.size());
                    componentArrays.assign_default(componentTypes.size());

                    const std::span tagTypes = reg.get_tag_types(entityId);

                    nodeStack.clear();
                    ptrStack.clear();

                    const u32 entityObject = doc.array_push_back(parentEntityObject);
                    doc.make_object(entityObject);

                    const u32 componentsObject = doc.child_object(entityObject, json_strings::componentsObject);

                    for (u32 j = 0; j < componentTypes.size(); ++j)
                    {
                        if (skipTypes.components.contains(componentTypes[j]))
                        {
                            continue;
                        }

                        const auto& componentTypeDesc = typeRegistry.get_component_type_desc(componentTypes[j]);

                        const auto componentNode = doc.child_object(componentsObject, componentTypeDesc.type.name);

                        auto* const propertyTree = propertyRegistry.try_get(componentTypeDesc.type);

                        if (!propertyTree)
                        {
                            continue;
                        }

                        const byte* const componentPtr = reg.try_get(entityId, componentTypes[j]);

                        nodeStack.assign(1, componentNode);
                        ptrStack.assign(1, componentPtr);

                        auto visitor = overload{
                            [&doc, &nodeStack, &ptrStack](const property_node& node, const property_node_start)
                            {
                                const byte* const ptr = ptrStack.back() + node.offset;
                                const auto newNode = doc.child_object(nodeStack.back(), sanitize_name(node.name));

                                ptrStack.push_back(ptr);
                                nodeStack.push_back(newNode);

                                return visit_result::recurse;
                            },
                            [&nodeStack, &ptrStack](const property_node&, const property_node_finish)
                            {
                                nodeStack.pop_back();
                                ptrStack.pop_back();
                            },
                            [&doc, &nodeStack, &ptrStack](const property_node& node,
                                const property_array& array,
                                auto&& visitElement)
                            {
                                byte* const arrayPtr = const_cast<byte*>(ptrStack.back() + node.offset);
                                const usize arraySize = array.size(arrayPtr);

                                const auto newNode = doc.child_array(nodeStack.back(), sanitize_name(node.name));

                                nodeStack.push_back(newNode);

                                for (usize i = 0; i < arraySize; ++i)
                                {
                                    byte* const e = static_cast<byte*>(array.at(arrayPtr, i));
                                    ptrStack.push_back(e);

                                    visitElement();

                                    ptrStack.pop_back();
                                }

                                nodeStack.pop_back();

                                return visit_result::sibling;
                            },
                            [&doc, &nodeStack, &ptrStack](const property& property)
                            {
                                const byte* const propertyPtr = ptrStack.back() + property.offset;

                                if (property.kind == property_kind::string)
                                {
                                    const auto parent = nodeStack.back();

                                    auto* const str = reinterpret_cast<const string*>(propertyPtr);

                                    OBLO_ASSERT(property.type == get_type_id<string>());

                                    doc.child_value(parent,
                                        sanitize_name(property.name),
                                        property_kind::string,
                                        as_bytes(data_string{.data = str->data(), .length = str->size()}));
                                }
                                else
                                {
                                    const auto parent = nodeStack.back();
                                    const auto [size, alignment] = get_size_and_alignment(property.kind);

                                    doc.child_value(parent,
                                        sanitize_name(property.name),
                                        property.kind,
                                        {propertyPtr, size});
                                }

                                return visit_result::recurse;
                            },
                        };

                        visit(*propertyTree, visitor);
                    }

                    for (u32 j = 0; j < tagTypes.size(); ++j)
                    {
                        const auto tagType = tagTypes[j];

                        if (!skipTypes.tags.contains(tagType))
                        {
                            const auto& tagDesc = typeRegistry.get_tag_type_desc(tagType);
                            doc.child_object(componentsObject, tagDesc.type.name);
                        }
                    }

                    if (auto* const cc = reg.try_get<children_component>(entityId); cc && !cc->children.empty())
                    {
                        const u32 childrenObject = doc.child_array(entityObject, json_strings::entityChildren);

                        for (auto child : cc->children)
                        {
                            stack.push_back({
                                .id = child,
                                .parentEntityObject = childrenObject,
                            });
                        }
                    }
                }
            }
        }

        return success_tag{};
    }

    expected<> read(ecs::entity_registry& reg,
        const data_document& doc,
        u32 docRoot,
        const property_registry& propertyRegistry,
        ecs::entity root,
        const read_config& cfg)
    {
        if (!doc.is_object(docRoot))
        {
            return "Invalid JSON structure: document root must be an object"_err;
        }

        const auto entities = doc.find_child(docRoot, json_strings::entitiesArray);

        if (!doc.is_array(entities))
        {
            return "Invalid JSON structure: entities must be an array"_err;
        }

        const auto& typeRegistry = reg.get_type_registry();

        buffered_array<ecs::component_type, 32> componentTypes;
        buffered_array<byte*, 32> componentPtrs;

        deque<u32> nodeStack;
        deque<byte*> ptrStack;

        struct entity_stack_entry
        {
            u32 entityObject;
            ecs::entity parent;
        };

        deque<entity_stack_entry> stack;

        if (root && doc.children_count(entities) > 0)
        {
            reg.add<children_component>(root);
        }

        for (const u32 rootEntityObject : doc.children(entities))
        {
            stack.assign(1,
                {
                    .entityObject = rootEntityObject,
                    .parent = root,
                });

            while (!stack.empty())
            {
                const auto [entityObject, parent] = stack.back();
                stack.pop_back();

                componentTypes.clear();
                componentPtrs.clear();

                ecs::component_and_tag_sets types = cfg.addTypes;

                const u32 childrenArray = doc.find_child(entityObject, json_strings::entityChildren);
                const u32 componentsObject = doc.find_child(entityObject, json_strings::componentsObject);

                // Iterate a first time to gather the component types
                for (const u32 componentObject : doc.children(componentsObject))
                {
                    const auto componentType = doc.get_node_name(componentObject);
                    const auto type = type_id{.name = componentType};

                    const auto componentTypeId = typeRegistry.find_component(type);

                    if (componentTypeId)
                    {
                        types.components.add(componentTypeId);
                    }
                    else if (const auto tagTypeId = typeRegistry.find_tag(type))
                    {
                        types.tags.add(tagTypeId);
                    }

                    componentTypes.push_back(componentTypeId);
                }

                // NOTE: The component sets have some extra components compared to the arrays because of this
                if (parent)
                {
                    types.components.add(typeRegistry.find_component<parent_component>());
                }

                if (childrenArray != data_node::Invalid)
                {
                    types.components.add(typeRegistry.find_component<children_component>());
                }

                ecs::entity entityId{};

                componentPtrs.resize_default(componentTypes.size());

                reg.create(types, 1, {&entityId, 1});
                reg.get(entityId, componentTypes, componentPtrs);

                u32 componentIndex = 0;

                for (const u32 component : doc.children(componentsObject))
                {
                    const auto componentTypeId = componentTypes[componentIndex];
                    const auto componentPtr = componentPtrs[componentIndex];

                    if (componentPtr)
                    {
                        auto* const propertyTree =
                            propertyRegistry.try_get(typeRegistry.get_component_type_desc(componentTypeId).type);

                        if (propertyTree)
                        {
                            nodeStack.assign(1, component);
                            ptrStack.assign(1, componentPtr);

                            auto visitor = overload{
                                [&doc, &nodeStack, &ptrStack](const property_node& node, const property_node_start)
                                {
                                    if (node.name == meta_properties::array_element)
                                    {
                                        return visit_result::recurse;
                                    }

                                    byte* const ptr = ptrStack.back() + node.offset;
                                    const auto newNode =
                                        doc.find_child(nodeStack.back(), hashed_string_view{node.name});

                                    // Push even if we early out, because the property_node_finish call will pop
                                    ptrStack.push_back(ptr);
                                    nodeStack.push_back(newNode);

                                    if (newNode == data_node::Invalid)
                                    {
                                        return visit_result::sibling;
                                    }

                                    OBLO_ASSERT(doc.is_object(newNode));

                                    return visit_result::recurse;
                                },
                                [&nodeStack, &ptrStack](const property_node& node, const property_node_finish)
                                {
                                    if (node.name != meta_properties::array_element)
                                    {
                                        nodeStack.pop_back();
                                        ptrStack.pop_back();
                                    }
                                },
                                [&doc, &nodeStack, &ptrStack](const property_node& node,
                                    const property_array& array,
                                    auto&& visitElement)
                                {
                                    const auto newNode = node.name.starts_with(meta_properties::prefix)
                                        ? nodeStack.back()
                                        : doc.find_child(nodeStack.back(), hashed_string_view{node.name});

                                    OBLO_ASSERT(doc.is_array(newNode));

                                    byte* const arrayPtr = ptrStack.back() + node.offset;
                                    usize arraySize = doc.children_count(newNode);

                                    if (array.optResize)
                                    {
                                        array.optResize(arrayPtr, arraySize);
                                    }
                                    else
                                    {
                                        arraySize = min(array.size(arrayPtr), arraySize);
                                    }

                                    nodeStack.push_back(newNode);

                                    u32 arrayElementNode = data_node::Invalid;

                                    for (usize i = 0; i < arraySize; ++i)
                                    {
                                        arrayElementNode = doc.child_next(newNode, arrayElementNode);
                                        nodeStack.push_back(arrayElementNode);

                                        byte* const e = static_cast<byte*>(array.at(arrayPtr, i));
                                        ptrStack.push_back(e);

                                        visitElement();

                                        ptrStack.pop_back();
                                        nodeStack.pop_back();
                                    }

                                    nodeStack.pop_back();

                                    return visit_result::sibling;
                                },
                                [&doc, &nodeStack, &ptrStack](const property& property)
                                {
                                    byte* const propertyPtr = ptrStack.back() + property.offset;

                                    const auto parent = nodeStack.back();

                                    const auto valueNode = property.name.starts_with(meta_properties::prefix)
                                        ? parent
                                        : doc.find_child(parent, hashed_string_view{property.name});

                                    if (valueNode != data_node::Invalid)
                                    {
                                        OBLO_ASSERT(doc.is_value(valueNode));

                                        switch (property.kind)
                                        {
                                        case property_kind::uuid:
                                            if (const auto value = doc.read_uuid(valueNode))
                                            {
                                                property_value_wrapper{*value}.assign_to(property.kind, propertyPtr);
                                            }
                                            break;

                                        case property_kind::string:
                                            if (const auto value = doc.read_string(valueNode))
                                            {
                                                property_value_wrapper{string_view{value->data, value->length}}
                                                    .assign_to(property.kind, propertyPtr);
                                            }
                                            break;

                                        case property_kind::boolean:
                                            if (const auto value = doc.read_bool(valueNode))
                                            {
                                                property_value_wrapper{*value}.assign_to(property.kind, propertyPtr);
                                            }
                                            break;

                                        case property_kind::f32:
                                            if (const auto value = doc.read_f32(valueNode))
                                            {
                                                property_value_wrapper{*value}.assign_to(property.kind, propertyPtr);
                                            }
                                            break;

                                        case property_kind::u8:
                                            if (const auto value = doc.read_u32(valueNode))
                                            {
                                                property_value_wrapper{narrow_cast<u8>(*value)}.assign_to(property.kind,
                                                    propertyPtr);
                                            }
                                            break;

                                        case property_kind::u16:
                                            if (const auto value = doc.read_u32(valueNode))
                                            {
                                                property_value_wrapper{narrow_cast<u16>(*value)}.assign_to(
                                                    property.kind,
                                                    propertyPtr);
                                            }
                                            break;

                                        case property_kind::u32:
                                            if (const auto value = doc.read_u32(valueNode))
                                            {
                                                property_value_wrapper{*value}.assign_to(property.kind, propertyPtr);
                                            }
                                            break;

                                        case property_kind::u64:
                                            if (const auto value = doc.read_u64(valueNode))
                                            {
                                                property_value_wrapper{*value}.assign_to(property.kind, propertyPtr);
                                            }
                                            break;

                                        default:
                                            OBLO_ASSERT(false);
                                            break;
                                        }
                                    }

                                    return visit_result::recurse;
                                },
                            };

                            visit(*propertyTree, visitor);
                        }
                    }

                    ++componentIndex;
                }

                if (parent)
                {
                    // Because of recursion we are reversing the order here, not a big deal for now
                    reg.get<children_component>(parent).children.emplace_back(entityId);
                    reg.get<parent_component>(entityId).parent = parent;
                }

                if (childrenArray != data_node::Invalid)
                {
                    u32 count{};

                    for (const u32 childObject : doc.children(childrenArray))
                    {
                        ++count;

                        stack.push_back({
                            .entityObject = childObject,
                            .parent = entityId,
                        });
                    }

                    reg.get<children_component>(entityId).children.reserve(count);
                }
            }
        }

        return success_tag{};
    }
}