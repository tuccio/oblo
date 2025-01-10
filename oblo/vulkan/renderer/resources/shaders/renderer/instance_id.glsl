#ifndef OBLO_INCLUDE_RENDERER_INSTANCE_ID
#define OBLO_INCLUDE_RENDERER_INSTANCE_ID

// We have 20 bits for the instance and 4 for the instance table, for a total of 24, which is the maximum we can use as
// instanceCustomIndex in the acceleration structures for ray-tracing
#define OBLO_INSTANCE_GLOBAL_ID_BITS 24 // We can only use 24 in the acceleration structures for RT
#define OBLO_INSTANCE_TABLE_ID_BITS 4   // We reserve 4 for the table id
#define OBLO_INSTANCE_ID_BITS 20        // And the remaining 20 for the instance index
#define OBLO_INSTANCE_ID_MASK 0xFFFFF   // Value of (1 << OBLO_VISIBILITY_BUFFER_INSTANCE_ID_BITS) - 1

void instance_parse_global_id(in uint globalInstanceId, out uint instanceTableId, out uint instanceId)
{
    instanceTableId = globalInstanceId >> OBLO_INSTANCE_ID_BITS;
    instanceId = globalInstanceId & OBLO_INSTANCE_ID_MASK;
}

uint instance_pack_global_id(in uint instanceTableId, in uint instanceId)
{
    return (instanceTableId << OBLO_INSTANCE_ID_BITS) | instanceId;
}

#endif