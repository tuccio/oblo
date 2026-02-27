#pragma once

#include <oblo/reflection/codegen/annotations.hpp>
#include <oblo/renderer/data/handles.hpp>

namespace oblo
{
    struct draw_mesh;

    struct draw_instance_component
    {
        mesh_handle mesh;
    } OBLO_COMPONENT("03fc22f7-111b-4971-8199-57d5a181360f", GpuComponent = "i_MeshHandles");

    struct draw_instance_id_component
    {
        // We use 24 bits, because that is what the ray tracing pipeline allows for custom ids
        // We reserve 4 for the instance table and 20 for the instance index
        u32 rtInstanceId;
    } OBLO_COMPONENT("6a10c5e0-aae7-4c3f-a8f4-30adb5d7a423");

    struct draw_mesh_component
    {
        h32<draw_mesh> mesh;
    } OBLO_COMPONENT("6e9b1a1a-39c3-470e-820b-a936f5e22d69");

    struct draw_raytraced_tag
    {
    } OBLO_TAG("85c8bdb7-8476-4ab1-b156-aae4b777aa2b");
}