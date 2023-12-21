#ifndef OBLO_INCLUDE_RENDERER_MESHES
#define OBLO_INCLUDE_RENDERER_MESHES

// These are required to use this header
// #extension GL_EXT_buffer_reference : require
// #extension GL_ARB_gpu_shader_int64 : require

// These need to match oblo::attribute_kind in C++
#define OBLO_VERTEX_ATTRIBUTE_POSITION 0
#define OBLO_VERTEX_ATTRIBUTE_NORMAL 1
#define OBLO_VERTEX_ATTRIBUTE_UV0 2
#define OBLO_VERTEX_ATTRIBUTE_MAX 32

#define OBLO_DESCRIPTOR_SET_MESH_DATABASE 0
#define OBLO_BINDING_MESH_DATABASE 64
#define OBLO_BINDING_MESH_HANDLES 65

struct mesh_table
{
    uint64_t deviceAddress;
    uint attributesMask;
    uint padding;
    uint attributeOffsets[OBLO_VERTEX_ATTRIBUTE_MAX];
};

layout(std430, binding = OBLO_BINDING_MESH_DATABASE) restrict readonly buffer b_MeshTables
{
    mesh_table g_MeshTables[];
};

struct mesh_handle
{
    uint value;
};

layout(std430, binding = OBLO_BINDING_MESH_HANDLES) restrict readonly buffer i_MeshHandles
{
    mesh_handle g_MeshHandles[];
};

uint get_mesh_table_index(in mesh_handle h)
{
    return h.value >> 24u;
}

uint get_mesh_index(in mesh_handle h)
{
    const uint mask = ~0u >> 8u;
    return (h.value & mask) - 1;
}

mesh_table get_mesh_table(in mesh_handle h)
{
    return g_MeshTables[get_mesh_table_index(h)];
}

struct vec3_attribute
{
    float x, y, z;
};

// Buffer references

layout(buffer_reference) buffer Vec2AttributeType
{
    vec2 values[];
};

layout(buffer_reference) buffer Vec3AttributeType
{
    vec3_attribute values[];
};

// Generic vertex data fetch

vec2 get_mesh_vec2_attribute(in mesh_table t, in uint attributeId, in uint vertexId)
{
    const uint64_t address = t.deviceAddress + t.attributeOffsets[attributeId];
    Vec2AttributeType attributeBuffer = Vec2AttributeType(address);
    return attributeBuffer.values[vertexId];
}

vec3 get_mesh_vec3_attribute(in mesh_table t, in uint attributeId, in uint vertexId)
{
    const uint64_t address = t.deviceAddress + t.attributeOffsets[attributeId];
    Vec3AttributeType attributeBuffer = Vec3AttributeType(address);
    const vec3_attribute a = attributeBuffer.values[vertexId];
    return vec3(a.x, a.y, a.z);
}

// Vertex attributes fetch

vec3 get_mesh_position(in mesh_table t, in uint vertexId)
{
    return get_mesh_vec3_attribute(t, OBLO_VERTEX_ATTRIBUTE_POSITION, vertexId);
}

vec3 get_mesh_normal(in mesh_table t, in uint vertexId)
{
    return get_mesh_vec3_attribute(t, OBLO_VERTEX_ATTRIBUTE_NORMAL, vertexId);
}

vec2 get_mesh_uv0(in mesh_table t, in uint vertexId)
{
    return get_mesh_vec2_attribute(t, OBLO_VERTEX_ATTRIBUTE_UV0, vertexId);
}

#endif