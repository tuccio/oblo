#ifndef OBLO_INCLUDE_RENDERER_ECS_ENTITY
#define OBLO_INCLUDE_RENDERER_ECS_ENTITY

struct ecs_entity
{
    uint id;
};

bool ecs_entity_is_valid(in ecs_entity e)
{
    return e.id != 0;
}

ecs_entity ecs_entity_invalid()
{
    ecs_entity e;
    e.id = 0;
    return e;
}

#endif