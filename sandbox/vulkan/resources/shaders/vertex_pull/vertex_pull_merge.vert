#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require

struct float3
{
    float x, y, z;
};

vec3 to_vec3(in const float3 f)
{
    return vec3(f.x, f.y, f.z);
}

layout(buffer_reference) buffer PositionBufferType
{
    float3 values[];
};

layout(buffer_reference) buffer ColorBufferType
{
    float3 values[];
};

layout(std430, binding = 0) buffer b_PositionBufferRefs
{
    uint64_t in_PositionBufferRefs[];
};

layout(std430, binding = 1) buffer b_ColorBufferRefs
{
    uint64_t in_ColorBufferRefs[];
};

layout(std430, binding = 2) buffer b_BatchIndex
{
    uint in_BatchIndex[];
};

layout(location = 0) out vec3 out_Color;

struct attributes
{
    vec3 position;
    vec3 color;
};

attributes read_attributes()
{
    const uint batchIndex = in_BatchIndex[gl_DrawID];

    PositionBufferType position = PositionBufferType(in_PositionBufferRefs[batchIndex]);
    ColorBufferType color = ColorBufferType(in_ColorBufferRefs[batchIndex]);

    attributes result;

    result.position = to_vec3(position.values[gl_VertexIndex]);
    result.color = to_vec3(color.values[gl_VertexIndex]);

    return result;
}

void main()
{
    const attributes inAttributes = read_attributes();
    gl_Position = vec4(inAttributes.position, 1.0);
    out_Color = inAttributes.color;
}