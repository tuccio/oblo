#include <oblo/scene/serialization/ecs_serializer.hpp>

#include <oblo/core/overload.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/ecs/archetype_storage.hpp>
#include <oblo/ecs/component_type_desc.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/visit.hpp>

namespace oblo::ecs_serializer
{
    expected<> write(data_document& doc,
        u32 docRoot,
        const ecs::entity_registry& reg,
        const property_registry& propertyRegistry,
        const write_config&)
    {
        const u32 entitiesArray = doc.child_array(docRoot, "entities");

        const auto& typeRegistry = reg.get_type_registry();
        const std::span archetypes = reg.get_archetypes();

        dynamic_array<u32> componentOffsets;
        componentOffsets.reserve(64);

        dynamic_array<byte*> componentArrays;
        componentArrays.reserve(64);

        for (const auto& archetype : archetypes)
        {
            const std::span componentTypes = ecs::get_component_types(archetype);
            componentOffsets.assign_default(componentTypes.size());
            componentArrays.assign_default(componentTypes.size());

            deque<u32> nodeStack;
            deque<byte*> ptrStack;

            ecs::for_each_chunk(archetype,
                componentTypes,
                componentOffsets,
                componentArrays,
                [&doc, entitiesArray, &componentTypes, &typeRegistry, &propertyRegistry, &nodeStack, &ptrStack](
                    const ecs::entity*,
                    std::span<std::byte*> componentArrays,
                    u32 numEntitiesInChunk)
                {
                    nodeStack.clear();
                    ptrStack.clear();

                    for (u32 i = 0; i < numEntitiesInChunk; ++i)
                    {
                        const u32 entityNode = doc.array_push_back(entitiesArray);
                        doc.make_object(entityNode);

                        for (u32 j = 0; j < componentTypes.size(); ++j)
                        {
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

                                    const auto newNode = node.isArray
                                        ? doc.child_array(nodeStack.back(), hashed_string_view{node.name})
                                        : doc.child_object(nodeStack.back(), hashed_string_view{node.name});

                                    nodeStack.push_back(newNode);

                                    return node.isArray ? visit_result::array_elements : visit_result::recurse;
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

                                    const auto newNode =
                                        doc.child_array(nodeStack.back(), hashed_string_view{node.name});

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
                                [&doc, &nodeStack, &ptrStack](const property_array& array,
                                    usize index,
                                    const property_array_element_start)
                                {
                                    byte* const ptr = ptrStack.back();

                                    if (index >= array.size(ptr))
                                    {
                                        return visit_result::terminate;
                                    }

                                    byte* const e = static_cast<byte*>(array.at(ptr, index));

                                    ptrStack.push_back(e);

                                    return visit_result::recurse;
                                },
                                [&nodeStack,
                                    &ptrStack](const property_array&, usize, const property_array_element_finish)
                                {
                                    ptrStack.pop_back();
                                    return visit_result::recurse;
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
                                            hashed_string_view{property.name},
                                            property_kind::string,
                                            as_bytes(data_string{.data = str->data(), .length = str->size()}));
                                    }
                                    else
                                    {
                                        const auto parent = nodeStack.back();
                                        const auto [size, alignment] = get_size_and_alignment(property.kind);

                                        doc.child_value(parent,
                                            hashed_string_view{property.name},
                                            property.kind,
                                            {propertyPtr, size});
                                    }

                                    return visit_result::recurse;
                                },
                            };

                            visit(*propertyTree, visitor);
                        }
                    }
                });
        }

        return success_tag{};
    }
}