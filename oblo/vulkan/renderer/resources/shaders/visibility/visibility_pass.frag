#version 460

layout(location = 0) flat in uvec2 in_VisibilityBufferData;

layout(location = 0) out uvec2 out_VisibilityBuffer;

void main()
{
    out_VisibilityBuffer = in_VisibilityBufferData;
}