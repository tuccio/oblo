#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_control_flow_attributes : require

#include <pathtracing/pathtracing>
#include <renderer/constants>
#include <renderer/geometry/barycentric>
#include <renderer/instance_id>
#include <renderer/instances>
#include <renderer/lights>
#include <renderer/material>
#include <renderer/meshes/mesh_attributes>
#include <renderer/meshes/mesh_data>
#include <renderer/meshes/mesh_indices_rt>
#include <renderer/meshes/mesh_table>
#include <renderer/random/random>
#include <renderer/random/sampling>
#include <renderer/shading/pbr_utility>
#include <renderer/textures>

layout(binding = 0) uniform b_LightConfig
{
    light_config g_LightConfig;
};

layout(std430, binding = 1) restrict readonly buffer b_LightData
{
    light_data g_Lights[];
};

layout(binding = 11) uniform accelerationStructureEXT u_SceneTLAS;

layout(location = 0) rayPayloadInEXT pathtracing_payload r_Payload;
layout(location = 1) rayPayloadEXT bool r_IsShadowed;

hitAttributeEXT vec2 h_BarycentricCoords;

// Multiple importance sampling heuristics
float mis_balance_heuristic(in float a, in float b)
{
    return a / (a + b);
}

float mis_power_heuristic(in float a, in float b)
{
    const float a2 = a * a;
    const float b2 = b * b;
    return a2 / (a2 + b2);
}

bool raytrace_visibility(in vec3 positionWS, in vec3 L, in float tMax)
{
    // Trace hard shadow by shooting a ray from the hit position towards the light
    const float tMin = 1e-2f;

    // No reason to call the hit shader, we only care about the miss shader
    const uint flags = gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

    // The miss shader will set it to false if no geometry is hit
    r_IsShadowed = true;

    traceRayEXT(u_SceneTLAS,
        flags,
        0xff, // cull mask
        0,    // STB record offset
        0,    // STB record stride
        1,    // Miss index
        positionWS,
        tMin,
        L,
        tMax,
        1 // payload location
    );

    return !r_IsShadowed;
}

void main()
{
    const uint globalInstanceId = gl_InstanceCustomIndexEXT;

    uint instanceTableId;
    uint instanceId;

    instance_parse_global_id(globalInstanceId, instanceTableId, instanceId);

    const mesh_handle mesh = OBLO_INSTANCE_DATA(instanceTableId, i_MeshHandles, instanceId);
    const gpu_material material = OBLO_INSTANCE_DATA(instanceTableId, i_MaterialBuffer, instanceId);

    barycentric_coords bc;
    bc.lambda = vec3(1.f - h_BarycentricCoords.x - h_BarycentricCoords.y, h_BarycentricCoords.x, h_BarycentricCoords.y);

    vec2 triangleUV0[3];
    vec3 trianglePosition[3];
    vec3 triangleNormal[3];

    // Read the mesh data
    const mesh_table meshTable = mesh_table_fetch(mesh);

    const uvec3 vertexIndices = mesh_get_primitive_indices(meshTable, mesh, gl_PrimitiveID);

    [[unroll]] for (uint i = 0; i < 3; ++i)
    {
        const uint vertexId = vertexIndices[i];
        trianglePosition[i] = mesh_get_position(meshTable, vertexId);
        triangleNormal[i] = mesh_get_normal(meshTable, vertexId);
        triangleUV0[i] = mesh_get_uv0(meshTable, vertexId);
    }

    const vec3 position = barycentric_interpolate(bc, trianglePosition);
    const vec3 normal = barycentric_interpolate(bc, triangleNormal);
    const vec2 uv0 = barycentric_interpolate(bc, triangleUV0);

    const vec3 positionWS = vec3(gl_ObjectToWorldEXT * vec4(position, 1.f));
    // NOTE: We may want to use the transpose inverse for normals
    const vec3 normalWS = normalize(vec3(normal * gl_WorldToObjectEXT));

    // TODO: Calculate the derivatives
    const vec2 uv0DDX = vec2(0);
    const vec2 uv0DDY = vec2(0);

    const pbr_material pbr = pbr_extract_parameters(material, uv0, uv0DDX, uv0DDY);

    const vec3 viewWS = normalize(gl_WorldRayOriginEXT - positionWS);

    if (g_LightConfig.lightsCount > 0)
    {
        const uint lightIndex = hash_pcg(r_Payload.seed) % g_LightConfig.lightsCount;
        const light_data light = g_Lights[lightIndex];

        float maxDistance = 1e6f;
        vec3 lightIntensity;
        vec3 L;

        if (light.type == OBLO_LIGHT_TYPE_DIRECTIONAL)
        {
            L = -light.direction;
            lightIntensity = light.intensity;
        }
        else if (light.type == OBLO_LIGHT_TYPE_POINT || light.type == OBLO_LIGHT_TYPE_SPOT)
        {
            const vec3 diskOffset = random_sample_uniform_disk(light.shadowPunctualRadius,
                light.direction,
                random_uniform_2d(r_Payload.seed));

            const vec3 target = light.position + diskOffset;

            const vec3 weightedDir = target - positionWS;
            const float attenuation = light_distance_attenuation(weightedDir, light.invSqrRadius);

            L = normalize(weightedDir);
            maxDistance = length(weightedDir);
            lightIntensity = attenuation * light.intensity;
        }

        const bool isVisible =
            (light.flags & OBLO_LIGHT_FLAG_SHADOW_CASTER) == 0 || raytrace_visibility(positionWS, L, maxDistance);

        if (isVisible)
        {
            const float cosTheta = max(dot(normalWS, L), 0.f);
            const vec3 f = pbr.albedo / float_pi();

            const float pdfLight = 1.f / float(g_LightConfig.lightsCount);
            const float pdfBsdf = cosTheta / float_pi();

            const float w = mis_power_heuristic(pdfLight, pdfBsdf);

            r_Payload.radiance += r_Payload.throughput * f * lightIntensity * cosTheta * w * float(g_LightConfig.lightsCount);
        }
    }

    r_Payload.radiance += r_Payload.throughput * pbr.emissive;

    // Just a Lambertian material for now, pdf cancels out with cosine sampling pdf
    r_Payload.throughput *= pbr.albedo;

    // Russian roulette
    const float rrMinProbability = .05f;

    // Use max component as survival probability
    const float p =
        clamp(max(r_Payload.throughput.r, max(r_Payload.throughput.g, r_Payload.throughput.b)), rrMinProbability, 1.f);

    // Random test
    if (random_uniform_1d(r_Payload.seed) > p)
    {
        r_Payload.done = true;
        return;
    }

    // Survived the russian roulette, so we compensate for the probability
    r_Payload.throughput /= p;

    // Cosine sampling because it's better for Lambertian at least
    r_Payload.direction = random_sample_cosine_hemisphere(normalWS, random_uniform_2d(r_Payload.seed));
    r_Payload.origin = positionWS;
}