#ifndef OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER
#define OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER

#define OBLO_VISIBILITY_BUFFER_MASK_INSTANCE_TABLE_ID 15 // 4 bits for instance tables

// We have 20 bits for the instance and 4 for the instance table, for a total of 24, which is the maximum we can use as
// instanceCustomIndex in the acceleration structures for ray-tracing
#define OBLO_VISIBILITY_BUFFER_INSTANCE_GLOBAL_ID_BITS 24 // We can only use 24 in the acceleration structures for RT
#define OBLO_VISIBILITY_BUFFER_INSTANCE_TABLE_ID_BITS 4   // We reserve 4 for the table id
#define OBLO_VISIBILITY_BUFFER_INSTANCE_ID_BITS 20        // And the remaining 20 for the instance index
#define OBLO_VISIBILITY_BUFFER_INSTANCE_ID_MASK 0xFFFFF   // Value of (1 << OBLO_VISIBILITY_BUFFER_INSTANCE_ID_BITS) - 1

#define OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID 8   // We keep 24 bits for the meshlet id
#define OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID 255 // And use the remaining 8 bits for the triangle id

struct visibility_buffer_data
{
    uint instanceTableId;
    uint instanceId;
    uint meshletId;
    uint meshletTriangleId;
};

void visibility_buffer_parse_instance_ids(in uint packedX, inout visibility_buffer_data r)
{
    r.instanceTableId = packedX >> OBLO_VISIBILITY_BUFFER_INSTANCE_ID_BITS;
    r.instanceId = packedX & OBLO_VISIBILITY_BUFFER_INSTANCE_ID_MASK;
}

void visibility_buffer_parse_meshlet_ids(in uint packedY, inout visibility_buffer_data r)
{
    r.meshletTriangleId = packedY & OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID;
    r.meshletId = packedY >> OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID;
}

uvec2 visibility_buffer_pack(in visibility_buffer_data data)
{
    uvec2 r;

    r.x = (data.instanceTableId << OBLO_VISIBILITY_BUFFER_INSTANCE_ID_BITS) | data.instanceId;

    r.y = (data.meshletTriangleId & OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID) |
        (data.meshletId << OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID);

    // Add one so that we can use 0 as invalid value
    r.y += 1;

    return r;
}

uint visibility_buffer_get_packed_instance_ids(in uvec2 visBuffer)
{
    return visBuffer.x;
}

uint visibility_buffer_get_packed_meshlet_ids(in uvec2 visBuffer)
{
    // Remove the +1 we added in the packing
    return visBuffer.y - 1;
}

bool visibility_buffer_parse(in uvec2 visBuffer, out visibility_buffer_data r)
{
    const bool valid = visBuffer.y != 0;

    const uint packedX = visibility_buffer_get_packed_instance_ids(visBuffer);
    visibility_buffer_parse_instance_ids(packedX, r);

    // Remove the +1 we added in the packing
    const uint packedY = visibility_buffer_get_packed_meshlet_ids(visBuffer);
    visibility_buffer_parse_meshlet_ids(packedY, r);

    return valid;
}

#endif