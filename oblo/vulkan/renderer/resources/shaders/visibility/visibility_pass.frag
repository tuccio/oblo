#version 460

#extension GL_EXT_debug_printf : require

layout(location = 0) flat in uvec2 in_VisibilityBufferData;
layout(location = 1) in vec2 in_DebugUV0;

layout(location = 0) out uvec2 out_VisibilityBuffer;
layout(location = 1) out vec4 out_DebugBuffer;

void main()
{
    // debugPrintfEXT("FRAG VIS: %u %u\n", in_VisibilityBufferData.x, in_VisibilityBufferData.y);

    out_VisibilityBuffer = in_VisibilityBufferData;
    out_DebugBuffer = vec4(in_DebugUV0, 0, 1);
}