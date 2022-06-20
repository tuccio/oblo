#version 450

struct float3
{
    float x, y, z;
};

vec3 to_vec3(in const float3 f)
{
    return vec3(f.x, f.y, f.z);
}

layout(std430, binding = 0) buffer b_Positions
{
    float3 in_Position[];
};

layout(std430, binding = 1) buffer b_Colors
{
    float3 in_Color[];
};

layout(location = 0) out vec3 out_Color;

struct transform_data
{
    vec3 translation;
    float scale;
};

layout(std430, binding = 0) buffer b_Transform
{
    transform_data in_Transform[];
};

void main()
{
    const vec3 position = to_vec3(in_Position[gl_VertexIndex]);
    const transform_data transform = in_Transform[gl_InstanceIndex];
    gl_Position = vec4(position * transform.scale + transform.translation, 1.0);
    out_Color = to_vec3(in_Color[gl_VertexIndex]);
}