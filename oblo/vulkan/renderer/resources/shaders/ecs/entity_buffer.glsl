#ifndef OBLO_INCLUDE_RENDERER_ECS_ENTITY_BUFFER
#define OBLO_INCLUDE_RENDERER_ECS_ENTITY_BUFFER

#include <ecs/entity>

layout(buffer_reference) buffer i_EntityIdBufferType
{
    ecs_entity values[];
};

#endif