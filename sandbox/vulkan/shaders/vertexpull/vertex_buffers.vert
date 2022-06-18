#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Color;

layout(location = 0) out vec3 out_Color;

layout(push_constant) uniform u_Constants
{
	vec3 translation;
	float scale;
} c_Constants;

void main()
{
    gl_Position = vec4(in_Position * c_Constants.scale + c_Constants.translation, 1.0);
    out_Color = in_Color;
}