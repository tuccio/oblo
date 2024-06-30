#ifndef OBLO_INCLUDE_RENDERER_BUFFER_REFERENCE_COMMON
#define OBLO_INCLUDE_RENDERER_BUFFER_REFERENCE_COMMON

// #extension GL_EXT_buffer_reference : require

struct vec3_attribute
{
    float x, y, z;
};

layout(buffer_reference) buffer Vec2AttributeType
{
    vec2 values[];
};

layout(buffer_reference) buffer Vec3AttributeType
{
    vec3_attribute values[];
};

layout(buffer_reference) buffer U32AttributeType
{
    uint values[];
};

#endif