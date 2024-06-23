#ifndef OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER
#define OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER

#define OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID 63 // 6 bits for instance tables
#define OBLO_VISIBILITY_BUFFER_SHIFT_INSTANCE_ID 6       // The rest are for instance id

struct visibility_buffer_data
{
    uint instanceTableId;
    uint instanceId;
    uint triangleIndex;
};

void visibility_buffer_parse(in uvec2 visBuffer, out visibility_buffer_data r)
{
    r.instanceTableId = visBuffer.x & OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID;
    r.instanceId = visBuffer.x >> OBLO_VISIBILITY_BUFFER_SHIFT_INSTANCE_ID;
    r.triangleIndex = visBuffer.y;
}

uvec2 visibility_buffer_pack(in visibility_buffer_data data)
{
    uvec2 r;

    r.x = (data.instanceTableId & OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID) |
        (data.instanceId << OBLO_VISIBILITY_BUFFER_SHIFT_INSTANCE_ID);

    r.y = data.triangleIndex;

    return r;
}

#endif