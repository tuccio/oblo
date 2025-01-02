#ifndef OBLO_INCLUDE_RENDERER_ECS_ENTITY_SET
#define OBLO_INCLUDE_RENDERER_ECS_ENTITY_SET

#include <ecs/entity>

const uint g_EcsEntityGenBits = 4;
const uint g_EcsEntityGenMask = 1 << (32 - g_EcsEntityGenBits);
const uint g_EcsEntityIndexMask = ~g_EcsEntityGenMask;

struct ecs_entity_set_entry
{
    ecs_entity entity;
    // Includes instance table id and instance id
    uint globalInstanceId;
};

layout(std430, binding = 70) restrict readonly buffer b_EcsEntitySet
{
    uint g_EcsEntityEntriesCount;
    ecs_entity_set_entry g_EcsEntitySet[];
};

bool ecs_entity_try_find(in ecs_entity e, out ecs_entity_set_entry entry)
{
    const uint index = e.id & g_EcsEntityIndexMask;

    if (index < g_EcsEntityEntriesCount)
    {
        entry = g_EcsEntitySet[index];
    }

    return ecs_entity_is_valid(e) && entry.entity.id == e.id;
}

#endif