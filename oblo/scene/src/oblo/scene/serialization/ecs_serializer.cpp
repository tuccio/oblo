#include <oblo/scene/serialization/ecs_serializer.hpp>

#include <oblo/core/overload.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/visit.hpp>

namespace oblo::ecs_serializer
{
    namespace
    {
        namespace json_strings
        {
            constexpr hashed_string_view entitiesArray = "entities";
        }

        hashed_string_view sanitize_name(string_view name)
        {
            return name.starts_with(notable_properties::prefix) ? hashed_string_view{} : hashed_string_view{name};
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
        const std::span archetypes = reg.get_archetypes();

        dynamic_array<u32> componentOffsets;
        componentOffsets.reserve(64);

        dynamic_array<byte*> componentArrays;
        componentArrays.reserve(64);

        deque<u32> nodeStack;
        deque<byte*> ptrStack;

        for (const auto& archetype : archetypes)
        {
            const auto componentsAndTags = get_component_and_tag_sets(archetype);

            if (!componentsAndTags.components.intersection(cfg.skipEntities.components).is_empty() ||
                !componentsAndTags.tags.intersection(cfg.skipEntities.tags).is_empty())
            {
                continue;
            }

            const std::span componentTypes = ecs::get_component_types(archetype);
            componentOffsets.assign_default(componentTypes.size());
            componentArrays.assign_default(componentTypes.size());

            const std::span tagTypes = ecs::get_tag_types(archetype);

            ecs::for_each_chunk(archetype,
                componentTypes,
                componentOffsets,
                componentArrays,
                [&cfg,
                    &doc,
                    entitiesArray,
                    &componentTypes,
                    &tagTypes,
                    &typeRegistry,
                    &propertyRegistry,
                    &nodeStack,
                    &ptrStack](const ecs::entity*, std::span<std::byte*> componentArrays, u32 numEntitiesInChunk)
                {
                    nodeStack.clear();
                    ptrStack.clear();

                    for (u32 i = 0; i < numEntitiesInChunk; ++i)
                    {
                        const u32 entityNode = doc.array_push_back(entitiesArray);
                        doc.make_object(entityNode);

                        for (u32 j = 0; j < componentTypes.size(); ++j)
                        {
                            if (cfg.skipTypes.components.contains(componentTypes[j]))
                            {
                                continue;
                            }

                            const auto& componentTypeDesc = typeRegistry.get_component_type_desc(componentTypes[j]);

                            const auto componentNode = doc.child_object(entityNode, componentTypeDesc.type.name);

                            auto* const propertyTree = propertyRegistry.try_get(componentTypeDesc.type);
                            OBLO_ASSERT(propertyTree);

                            if (!propertyTree)
                            {
                                continue;
                            }

                            byte* const componentPtr = componentArrays[j] + i * componentTypeDesc.size;

                            nodeStack.assign(1, componentNode);
                            ptrStack.assign(1, componentPtr);

                            auto visitor = overload{
                                [&doc, &propertyTree, &nodeStack, &ptrStack](const property_node& node,
                                    const property_node_start)
                                {
                                    byte* const ptr = ptrStack.back() + node.offset;

                                    ptrStack.push_back(ptr);

                                    const auto newNode = doc.child_object(nodeStack.back(), sanitize_name(node.name));

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
                                    byte* const arrayPtr = ptrStack.back() + node.offset;
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
                                    byte* const propertyPtr = ptrStack.back() + property.offset;

                                    std::span<const byte> propertyData;

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

                            if (!cfg.skipTypes.tags.contains(tagType))
                            {
                                doc.child_object(entityNode, typeRegistry.get_tag_type_desc(tagType).type.name);
                            }
                        }
                    }
                });
        }

        return success_tag{};
    }

    expected<> read(
        ecs::entity_registry& reg, const data_document& doc, u32 docRoot, const property_registry& propertyRegistry)
    {
        if (!doc.is_object(docRoot))
        {
            return unspecified_error_tag{};
        }

        const auto entities = doc.find_child(docRoot, json_strings::entitiesArray);

        if (!doc.is_array(entities))
        {
            return unspecified_error_tag{};
        }

        const auto& typeRegistry = reg.get_type_registry();

        buffered_array<ecs::component_type, 32> componentTypes;
        buffered_array<byte*, 32> componentPtrs;

        deque<u32> nodeStack;
        deque<byte*> ptrStack;

        for (u32 child = doc.child_next(entities, data_node::Invalid); child != data_node::Invalid;
             child = doc.child_next(entities, child))
        {
            componentTypes.clear();
            componentPtrs.clear();

            ecs::component_and_tag_sets types{};

            for (u32 component = doc.child_next(child, data_node::Invalid); component != data_node::Invalid;
                 component = doc.child_next(child, component))
            {
                const auto componentType = doc.get_node_name(component);
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

            ecs::entity entityId{};

            componentPtrs.resize_default(componentTypes.size());

            reg.create(types, 1, {&entityId, 1});
            reg.get(entityId, componentTypes, componentPtrs);

            for (u32 component = doc.child_next(child, data_node::Invalid), componentIndex = 0;
                 component != data_node::Invalid;
                 ++componentIndex, component = doc.child_next(child, component))
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
                            [&doc, &propertyTree, &nodeStack, &ptrStack](const property_node& node,
                                const property_node_start)
                            {
                                if (node.name == notable_properties::array_element)
                                {
                                    return visit_result::recurse;
                                }

                                byte* const ptr = ptrStack.back() + node.offset;

                                ptrStack.push_back(ptr);

                                const auto newNode = doc.find_child(nodeStack.back(), hashed_string_view{node.name});
                                OBLO_ASSERT(doc.is_object(newNode));

                                nodeStack.push_back(newNode);

                                return visit_result::recurse;
                            },
                            [&nodeStack, &ptrStack](const property_node& node, const property_node_finish)
                            {
                                if (node.name != notable_properties::array_element)
                                {
                                    nodeStack.pop_back();
                                    ptrStack.pop_back();
                                }
                            },
                            [&doc, &nodeStack, &ptrStack](const property_node& node,
                                const property_array& array,
                                auto&& visitElement)
                            {
                                const auto newNode = node.name.starts_with(notable_properties::prefix)
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

                                std::span<const byte> propertyData;

                                const auto parent = nodeStack.back();

                                const auto valueNode = property.name.starts_with(notable_properties::prefix)
                                    ? parent
                                    : doc.find_child(parent, hashed_string_view{property.name});

                                if (valueNode != data_node::Invalid)
                                {
                                    OBLO_ASSERT(doc.is_value(valueNode));

                                    switch (property.kind)
                                    {
                                    case property_kind::string:
                                        if (const auto value = doc.read_string(valueNode))
                                        {
                                            *reinterpret_cast<string*>(propertyPtr) =
                                                string_view{value->data, value->length};
                                        }
                                        break;

                                    case property_kind::boolean:
                                        if (const auto value = doc.read_bool(valueNode))
                                        {
                                            *reinterpret_cast<bool*>(propertyPtr) = *value;
                                        }
                                        break;

                                    case property_kind::f32:
                                        if (const auto value = doc.read_f32(valueNode))
                                        {
                                            *reinterpret_cast<f32*>(propertyPtr) = *value;
                                        }
                                        break;

                                    case property_kind::u32:
                                        if (const auto value = doc.read_u32(valueNode))
                                        {
                                            *reinterpret_cast<u32*>(propertyPtr) = *value;
                                        }
                                        break;

                                    default:
                                        break;
                                    }
                                }

                                return visit_result::recurse;
                            },
                        };

                        visit(*propertyTree, visitor);
                    }
                }
            }
        }

        return success_tag{};
    }
}