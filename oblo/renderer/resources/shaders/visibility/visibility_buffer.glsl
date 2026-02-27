#ifndef OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER
#define OBLO_INCLUDE_VISIBILITY_VISIBILITY_BUFFER

#include <renderer/instance_id>

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
    instance_parse_global_id(packedX, r.instanceTableId, r.instanceId);
}

void visibility_buffer_parse_meshlet_ids(in uint packedY, inout visibility_buffer_data r)
{
    r.meshletTriangleId = packedY & OBLO_VISIBILITY_BUFFER_MASK_TRIANGLE_ID;
    r.meshletId = packedY >> OBLO_VISIBILITY_BUFFER_SHIFT_MESHLET_ID;
}

uvec2 visibility_buffer_pack(in visibility_buffer_data data)
{
    uvec2 r;

    r.x = instance_pack_global_id(data.instanceTableId, data.instanceId);

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