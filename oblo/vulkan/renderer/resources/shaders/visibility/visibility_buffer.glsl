#ifndef OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER
#define OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER

#define OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID 15 // 4 bits for instance tables
#define OBLO_VISIBILITY_BUFFER_SHIFT_INSTANCE_ID 4       // The rest are for instance id

#define OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID 8   // We keep 24 bits for the meshlet id
#define OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID 255 // And use the remaining 8 bits for the triangle id

struct visibility_buffer_data
{
    uint instanceTableId;
    uint instanceId;
    uint meshletId;
    uint meshletTriangleId;
};

bool visibility_buffer_parse(in uvec2 visBuffer, out visibility_buffer_data r)
{
    const bool valid = visBuffer.y != 0;

    r.instanceTableId = visBuffer.x & OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID;
    r.instanceId = visBuffer.x >> OBLO_VISIBILITY_BUFFER_SHIFT_INSTANCE_ID;

    // Remove the +1 we added in the packing
    const uint packedY = visBuffer.y - 1;

    r.meshletTriangleId = packedY & OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID;
    r.meshletId = packedY >> OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID;

    return valid;
}

uvec2 visibility_buffer_pack(in visibility_buffer_data data)
{
    uvec2 r;

    r.x = (data.instanceTableId & OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID) |
        (data.instanceId << OBLO_VISIBILITY_BUFFER_SHIFT_INSTANCE_ID);

    r.y = (data.meshletTriangleId & OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID) |
        (data.meshletId << OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID);

    // Add one so that we can use 0 as invalid value
    r.y += 1;

    return r;
}

#endif