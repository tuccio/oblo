#include <oblo/scene/serialization/ecs_serializer.hpp>

#include <oblo/core/finally.hpp>
#include <oblo/core/overload.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/tag_type_desc.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/visit.hpp>

namespace oblo::ecs_serializer
{
    namespace
    {
        namespace json_strings
        {
            constexpr hashed_string_view entitiesArray = "entities";
            constexpr hashed_string_view entityId = "id";
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
        const std::span archetypes = reg.get_archetypes();

        dynamic_array<u32> componentOffsets;
        componentOffsets.reserve(64);

        dynamic_array<byte*> componentArrays;
        componentArrays.reserve(64);

        deque<u32> nodeStack;
        deque<byte*> ptrStack;

        /*
        * When deserializing we need to be able to map the file id to the entity id
        Change the format to be like this :

        // Use 0 as null value for file ids
        {
            "entities": [
                {
                    "id": 1,
                    "components": {
                        "hierarchy_component": { "parent": 0 } ..., // Ignore the other fields, this needs special
        handling "transform_component": ...,
                       ....
                    }
                }
            ],
        }

        When deserializing:
            - Keep doing what we do: deserialize everything
                - If a component might have an entity reference:
                    - Deserialize reading the file id from json
                    - Push the number of created entities in the archetype, we need to do a post resolve later

            - After regular serialization is done:
                - For each archetype pushed earlier
                    - If the component type might have an entity reference:
                        - Patch using the lookup



        */

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
                [&reg,
                    &cfg,
                    &doc,
                    entitiesArray,
                    &componentTypes,
                    &tagTypes,
                    &typeRegistry,
                    &propertyRegistry,
                    &nodeStack,
                    &ptrStack](const ecs::entity* entities,
                    std::span<std::byte*> componentArrays,
                    u32 numEntitiesInChunk)
                {
                    nodeStack.clear();
                    ptrStack.clear();

                    for (u32 i = 0; i < numEntitiesInChunk; ++i)
                    {
                        const u32 entityObject = doc.array_push_back(entitiesArray);
                        doc.make_object(entityObject);

                        const ecs::entity entityId = entities[i];
                        const u32 entityFileId = reg.extract_entity_index(entityId);

                        doc.child_value(entityObject, json_strings::entityId, property_value_wrapper{entityFileId});

                        const u32 componentsObject = doc.child_object(entityObject, json_strings::componentsObject);

                        for (u32 j = 0; j < componentTypes.size(); ++j)
                        {
                            if (cfg.skipTypes.components.contains(componentTypes[j]))
                            {
                                continue;
                            }

                            const auto& componentTypeDesc = typeRegistry.get_component_type_desc(componentTypes[j]);

                            const auto componentNode = doc.child_object(componentsObject, componentTypeDesc.type.name);

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
                                [&doc, &nodeStack, &ptrStack](const property_node& node, const property_node_start)
                                {
                                    byte* const ptr = ptrStack.back() + node.offset;
                                    const auto newNode = doc.child_object(nodeStack.back(), sanitize_name(node.name));

                                    ptrStack.push_back(ptr);
                                    nodeStack.push_back(newNode);

                                    return visit_result::recurse;
                                },
                                [&doc, &nodeStack, &ptrStack](const property_node&, const property_node_finish)
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
                                [&reg, &doc, &nodeStack, &ptrStack](const property& property)
                                {
                                    byte* const propertyPtr = ptrStack.back() + property.offset;

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
                                    else if (property.kind == property_kind::h32 &&
                                        property.type == get_type_id<ecs::entity>())
                                    {
                                        const auto parent = nodeStack.back();
                                        auto* const e = reinterpret_cast<const ecs::entity*>(propertyPtr);

                                        doc.child_value(parent,
                                            sanitize_name(property.name),
                                            property_value_wrapper{ecs::entity{reg.extract_entity_index(*e)}});
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
                                const auto& tagDesc = typeRegistry.get_tag_type_desc(tagType);
                                doc.child_object(componentsObject, tagDesc.type.name);
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

        struct file_entity_id;
        h32_flat_extpool_dense_map<file_entity_id, ecs::entity> fileIdToEntityId;

        final_act_queue deferred;

        for (const u32 entityObject : doc.children(entities))
        {
            componentTypes.clear();
            componentPtrs.clear();

            ecs::component_and_tag_sets types{};

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

            ecs::entity entityId{};

            componentPtrs.resize_default(componentTypes.size());

            reg.create(types, 1, {&entityId, 1});
            reg.get(entityId, componentTypes, componentPtrs);

            const expected id = doc.read_u32(doc.find_child(entityObject, json_strings::entityId));

            if (id && *id != 0)
            {
                fileIdToEntityId.emplace(h32<file_entity_id>{*id}, entityId);
            }

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
                                const auto newNode = doc.find_child(nodeStack.back(), hashed_string_view{node.name});

                                OBLO_ASSERT(doc.is_object(newNode));

                                ptrStack.push_back(ptr);
                                nodeStack.push_back(newNode);

                                return visit_result::recurse;
                            },
                            [&doc, &nodeStack, &ptrStack](const property_node& node, const property_node_finish)
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
                            [&deferred, &fileIdToEntityId, &doc, &nodeStack, &ptrStack](const property& property)
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
                                            property_value_wrapper{string_view{value->data, value->length}}.assign_to(
                                                property.kind,
                                                propertyPtr);
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

                                    case property_kind::h32:
                                        if (property.type == get_type_id<ecs::entity>())
                                        {
                                            const expected fileId = doc.read_u32(valueNode);

                                            if (fileId)
                                            {
                                                auto* const entityRef = new (propertyPtr) ecs::entity{*fileId};

                                                // This is currently banking on pointers being stable, which is the
                                                // case right now but not very future-proof
                                                deferred.push(
                                                    [entityRef, &fileIdToEntityId]
                                                    {
                                                        ecs::entity* const e = fileIdToEntityId.try_find(
                                                            h32<file_entity_id>{entityRef->value});

                                                        *entityRef = e ? *e : ecs::entity{};
                                                    });
                                            }

                                            break;
                                        }

                                        // If it's not an entity, treat it like any u32
                                        [[fallthrough]];

                                    case property_kind::u32:
                                        if (const auto value = doc.read_u32(valueNode))
                                        {
                                            property_value_wrapper{*value}.assign_to(property.kind, propertyPtr);
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

                ++componentIndex;
            }
        }

        return success_tag{};
    }
}