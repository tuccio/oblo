#version 450 core

#extension GL_EXT_nonuniform_qualifier : require

#include <renderer/textures>

layout(location = 0) out vec4 out_Color;

layout(location = 0) in struct
{
    vec4 color;
    float cornerRadius;
    vec2 position;
    vec2 halfSize;
    vec2 uv;
} in_Data;

layout(location = 5) flat in uint in_TextureID;

float rounded_box_signed_distance(in vec2 p, in vec2 halfSize, in float r)
{
    const vec2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0) - r;
}

void main()
{
    // Textured quads (glyphs) sample the atlas through the global bindless descriptor set
    // (g_Textures2D) using the bindless handle carried per-instance. A textureId of 0 means
    // a solid rectangle.
    if (in_TextureID != 0u)
    {
        const uint idx = texture_get_index(in_TextureID);
        const float coverage =
            texture(sampler2D(g_Textures2D[nonuniformEXT(idx)], g_Samplers[OBLO_SAMPLER_LINEAR_CLAMP_EDGE]), in_Data.uv)
                .r;

        if (coverage <= 0.0)
        {
            discard;
        }

        out_Color = vec4(in_Data.color.rgb, in_Data.color.a * coverage);
        return;
    }

    const float d = rounded_box_signed_distance(in_Data.position, in_Data.halfSize, in_Data.cornerRadius);

    if (d > 0)
    {
        discard;
    }

    out_Color = in_Data.color;
}
