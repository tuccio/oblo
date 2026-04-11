#ifndef OBLO_INCLUDE_RENDERER_DEFERRED_LIGHTING_GBUFFER
#define OBLO_INCLUDE_RENDERER_DEFERRED_LIGHTING_GBUFFER

struct gbuffer0
{
    vec3 position;
    float roughness;
};

struct gbuffer1
{
    vec3 normal;
};

struct gbuffer2
{
    vec3 albedo;
    float metalness;
};

struct gbuffer3
{
    vec3 emissive;
    float ior;
};

vec4 gbuffer_pack(in gbuffer0 g)
{
    return vec4(g.position, g.roughness);
}

vec4 gbuffer_pack(in gbuffer1 g)
{
    return vec4(g.normal, 0.f);
}

vec4 gbuffer_pack(in gbuffer2 g)
{
    return vec4(g.albedo, g.metalness);
}

vec4 gbuffer_pack(in gbuffer3 g)
{
    return vec4(g.emissive, g.ior);
}

void gbuffer_unpack(out gbuffer0 g, in vec4 t)
{
    g.position = t.xyz;
    g.roughness = t.w;
}

void gbuffer_unpack(out gbuffer1 g, in vec4 t)
{
    g.normal = t.xyz;
}

void gbuffer_unpack(out gbuffer2 g, in vec4 t)
{
    g.albedo = t.xyz;
    g.metalness = t.w;
}

void gbuffer_unpack(out gbuffer3 g, in vec4 t)
{
    g.emissive = t.xyz;
    g.ior = t.w;
}

#endif