#if defined(OBLO_STAGE_VERTEX)

layout(push_constant) uniform c_VertexPushConstants
{
    vec2 scale;
    vec2 translation;
}
g_Constants;

#elif defined(OBLO_STAGE_FRAGMENT)

layout(push_constant) uniform c_FragmentPushConstants
{
    layout(offset = 16) uint textureId;
}
g_Constants;

#endif