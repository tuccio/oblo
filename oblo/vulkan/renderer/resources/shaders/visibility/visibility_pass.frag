#version 460

#extension GL_EXT_debug_printf : require

layout(location = 0) flat in uvec2 in_VisibilityBufferData;

layout(location = 0) out uvec2 out_VisibilityBuffer;

void main()
{
    // debugPrintfEXT("Fragment shader! %u %u\n", in_VisibilityBufferData.x, in_VisibilityBufferData.y);

    // out_VisibilityBuffer = in_VisibilityBufferData;
    out_VisibilityBuffer = uvec2(0, 1);
}