#ifndef OBLO_INCLUDE_RENDERER_RAYTRACING_COMMON
#define OBLO_INCLUDE_RENDERER_RAYTRACING_COMMON

struct rt_instance_id
{
    uint instanceTableId;
    uint instanceId;
};

rt_instance_id rt_instance_id_from_custom_index(uint customIndex)
{
    rt_instance_id r;
    r.instanceTableId = customIndex >> 20;
    r.instanceId = customIndex & ((1u << 20) - 1);
    return r;
}

#endif