#include <oblo/scene/serialization/ecs_serializer.hpp>

#include <oblo/core/overload.hpp>
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

            ecs::for_each_chunk(archetype,
                componentTypes,
                componentOffsets,
                componentArrays,
                [&doc, entitiesArray, &componentTypes, &typeRegistry, &propertyRegistry](const ecs::entity*,
                    std::span<std::byte*> componentArrays,
                    u32 numEntitiesInChunk)
                {
                    for (u32 i = 0; i < numEntitiesInChunk; ++i)
                    {
                        const u32 entityNode = doc.array_push_back(entitiesArray);
                        doc.make_object(entityNode);

                        deque<u32> nodeStack;

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

                            // TODO
                            byte* const componentPtr = componentArrays[j];

                            byte* ptr = componentPtr;

                            nodeStack.assign(1, componentNode);

                            visit(*propertyTree,
                                overload{
                                    [&ptr, &doc, &nodeStack](const property_node& node, const property_node_start)
                                    {
                                        ptr += node.offset;
                                        const auto newNode =
                                            doc.child_object(nodeStack.back(), hashed_string_view{node.name});
                                        nodeStack.push_back(newNode);
                                        return visit_result::recurse;
                                    },
                                    [&ptr, &nodeStack](const property_node& node, const property_node_finish)
                                    {
                                        ptr -= node.offset;
                                        nodeStack.pop_back();
                                    },
                                    [&ptr, &doc, &nodeStack](const property& property)
                                    {
                                        byte* const propertyPtr = ptr + property.offset;

                                        std::span<const byte> propertyData;

                                        if (property.kind == property_kind::string)
                                        {
                                            // TODO
                                        }
                                        else
                                        {
                                            const auto [size, alignment] = get_size_and_alignment(property.kind);

                                            doc.child_value(nodeStack.back(),
                                                hashed_string_view{property.name},
                                                property.kind,
                                                {propertyPtr, size});
                                        }

                                        return visit_result::recurse;
                                    },
                                });
                        }
                    }
                });
        }

        return success_tag{};
    }
}