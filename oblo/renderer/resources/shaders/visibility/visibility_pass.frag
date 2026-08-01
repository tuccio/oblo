#version 460

#extension GL_EXT_mesh_shader : enable

layout(location = 0) perprimitiveEXT flat in uvec2 in_VisibilityBufferData;

layout(location = 0) out uvec2 out_VisibilityBuffer;

void main()
{
    out_VisibilityBuffer = in_VisibilityBufferData;
}