#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>

namespace oblo::vk::main_view
{
    constexpr string_view InPickingConfiguration{"Picking Configuration"};
    constexpr string_view InResolution{"Resolution"};
    constexpr string_view InCamera{"Camera"};
    constexpr string_view InTime{"Time"};
    constexpr string_view InMeshDatabase{"MeshDatabase"};
    constexpr string_view InInstanceTables{"InstanceTables"};
    constexpr string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr string_view InFinalRenderTarget{"FinalRenderTarget"};

    constexpr string_view InLights{"LightData"};
    constexpr string_view InLightBuffer{"LightDataBuffer"};
    constexpr string_view InLightConfig{"LightConfig"};
    constexpr string_view InLastFrameSurfelsGrid{"LastFrameSurfelsGrid"};
    constexpr string_view InLastFrameSurfelsPool{"LastFrameSurfelsPool"};
    constexpr string_view InUpdatedSurfelsGrid{"UpdatedSurfelsGrid"};
    constexpr string_view InUpdatedSurfelsPool{"UpdatedSurfelsPool"};
    constexpr string_view InShadowSink{"ShadowSink"};

    constexpr string_view InDebugMode{"Debug Mode"};

    constexpr string_view OutResolution{"Resolution"};
    constexpr string_view OutCameraBuffer{"CameraBuffer"};
    constexpr string_view OutDepthBuffer{"DepthBuffer"};
    constexpr string_view OutVisibilityBuffer{"VisibilityBuffer"};
    constexpr string_view OutLitImage{"LitOutput"};
    constexpr string_view OutDebugImage{"DebugOutput"};
    constexpr string_view OutGIDebugImage{"DebugGIOutput"};
    constexpr string_view OutRTDebugImage{"DebugRTOutput"};
    constexpr string_view OutSurfelsTileCoverageSink{"SurfelsGITileOutput"};

    constexpr string_view OutPicking{"OutPicking"};

    struct config
    {
        bool withPicking{false};
        bool withSurfelsGI{false};
    };

    frame_graph_template create(const frame_graph_registry& registry, const config& cfg = {});
}

namespace oblo::vk::scene_data
{
    constexpr string_view InLights{"LightData"};
    constexpr string_view OutLights{"LightData"};
    constexpr string_view OutLightBuffer{"LightDataBuffer"};
    constexpr string_view OutLightConfig{"LightConfig"};
    constexpr string_view OutMeshDatabase{"MeshDatabase"};
    constexpr string_view OutInstanceTables{"InstanceTables"};
    constexpr string_view OutInstanceBuffers{"InstanceBuffers"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk::raytraced_shadow_view
{
    constexpr string_view InCameraBuffer{"CameraBuffer"};
    constexpr string_view InLightBuffer{"LightBuffer"};
    constexpr string_view InResolution{"Resolution"};
    constexpr string_view InConfig{"Config"};
    constexpr string_view InDepthBuffer{"DepthBuffer"};
    constexpr string_view InVisibilityBuffer{"VisibilityBuffer"};
    constexpr string_view InMeshDatabase{"MeshDatabase"};
    constexpr string_view InInstanceTables{"InstanceTables"};
    constexpr string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr string_view OutShadow{"Shadow"};
    constexpr string_view OutShadowSink{"ShadowSink"};

    frame_graph_template create(const frame_graph_registry& registry);

    type_id get_main_view_barrier_source();
    type_id get_main_view_barrier_target();
}

namespace oblo::vk::surfels_gi
{
    constexpr string_view InCameraBuffer{"CameraBuffer"};
    constexpr string_view InGridBounds{"GridBounds"};
    constexpr string_view InGridCellSize{"GridCellSize"};
    constexpr string_view InMaxSurfels{"MaxSurfels"};
    constexpr string_view InTileCoverageSink{"TileCoverageSink"};
    constexpr string_view OutLastFrameGrid{"LastGrid"};
    constexpr string_view OutLastFramePool{"LastPool"};
    constexpr string_view OutUpdatedFrameGrid{"UpdatedGrid"};
    constexpr string_view OutUpdatedFramePool{"UpdatedPool"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry();
}