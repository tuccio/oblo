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

    constexpr string_view InRenderWorld{"RenderWorld"};
    constexpr string_view InLights{"LightData"};
    constexpr string_view InLightBuffer{"LightDataBuffer"};
    constexpr string_view InLightConfig{"LightConfig"};
    constexpr string_view InLastFrameSurfelsGrid{"LastFrameSurfelsGrid"};
    constexpr string_view InLastFrameSurfelsGridData{"LastFrameSurfelsGridData"};
    constexpr string_view InLastFrameSurfelData{"LastFrameSurfelData"};
    constexpr string_view InLastFrameSurfelSpawnData{"LastFrameSurfelSpawnData"};
    constexpr string_view InLastFrameSurfelsLightingData{"LastFrameSurfelsLightingData"};
    constexpr string_view InUpdatedSurfelsGrid{"UpdatedSurfelsGrid"};
    constexpr string_view InUpdatedSurfelsGridData{"UpdatedSurfelsGridData"};
    constexpr string_view InUpdatedSurfelsData{"UpdatedSurfelsData"};
    constexpr string_view InUpdatedSurfelsLightingData{"UpdatedSurfelsLightingData"};
    constexpr string_view InUpdatedSurfelsLightEstimatorData{"UpdatedSurfelsLightEstimatorData"};
    constexpr string_view InSurfelsLastUsage{"SurfelsLastUsage"};
    constexpr string_view InShadowSink{"ShadowSink"};
    constexpr string_view InSkyboxSettingsBuffer{"SkyboxSettingsBuffer"};

    constexpr string_view InRTAOBias{"RTAOBias"};
    constexpr string_view InRTAOMaxDistance{"RTAOMaxDistance"};
    constexpr string_view InRTAOMaxHistoryWeight{"RTAOMaxHistoryWeight"};

    constexpr string_view InDebugMode{"Debug Mode"};

    constexpr string_view OutResolution{"Resolution"};
    constexpr string_view OutCameraDataSink{"CameraDataSink"};
    constexpr string_view OutCameraBuffer{"CameraBuffer"};
    constexpr string_view OutDepthBuffer{"DepthBuffer"};
    constexpr string_view OutLastFrameDepthBuffer{"LastFrameDepthBuffer"};
    constexpr string_view OutVisibilityBuffer{"VisibilityBuffer"};
    constexpr string_view OutLitImage{"LitOutput"};
    constexpr string_view OutDebugImage{"DebugOutput"};
    constexpr string_view OutGISurfelsImage{"DebugGISurfelsOutput"};
    constexpr string_view OutGiSurfelsLightingImage{"DebugGITileCoverageOutput"};
    constexpr string_view OutGiSurfelsRayCount{"DebugtGiSurfelsRayCount"};
    constexpr string_view OutGiSurfelsInconsistency{"DebugtGiSurfelsInconsistency"};
    constexpr string_view OutGiSurfelsLifetime{"DebugtGiSurfelsLifetime"};
    constexpr string_view OutRTDebugImage{"DebugRTOutput"};
    constexpr string_view OutSurfelsTileCoverageSink{"SurfelsGITileOutput"};
    constexpr string_view OutDisocclusionMask{"DisocclusionMask"};
    constexpr string_view OutMotionVectors{"MotionVectors"};
    constexpr string_view OutRTAmbientOcclusion{"RT AmbientOcclusion"};
    constexpr string_view OutAmbientOcclusion{"AmbientOcclusion"};

    constexpr string_view OutPicking{"OutPicking"};

    struct config
    {
        bool withPicking{false};
        bool withSurfelsGI{true};
    };

    frame_graph_template create(const frame_graph_registry& registry, const config& cfg = {});
}

namespace oblo::vk::raytraced_ambient_occlusion
{
    constexpr string_view InCameraBuffer{"CameraBuffer"};
    constexpr string_view InLightBuffer{"LightBuffer"};
    constexpr string_view InVisibilityBuffer{"VisibilityBuffer"};
    constexpr string_view InMeshDatabase{"MeshDatabase"};
    constexpr string_view InInstanceTables{"InstanceTables"};
    constexpr string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr string_view InDisocclusionMask{"DisocclusionMask"};
    constexpr string_view InMotionVectors{"MotionVectors"};

    constexpr string_view OutAmbientOcclusion{"AmbientOcclusion"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk::scene_data
{
    constexpr string_view InRenderWorld{"RenderWorld"};
    constexpr string_view InLights{"LightData"};
    constexpr string_view InSkyboxResource{"InSkyboxResource"};
    constexpr string_view InSkyboxSettings{"SkyboxSettings"};

    constexpr string_view OutRenderWorld{"RenderWorld"};
    constexpr string_view OutLights{"LightData"};
    constexpr string_view OutLightBuffer{"LightDataBuffer"};
    constexpr string_view OutLightConfig{"LightConfig"};
    constexpr string_view OutMeshDatabase{"MeshDatabase"};
    constexpr string_view OutInstanceTables{"InstanceTables"};
    constexpr string_view OutInstanceBuffers{"InstanceBuffers"};
    constexpr string_view OutEcsEntitySetBuffer{"EcsEntitySetBuffer"};
    constexpr string_view OutSkyboxSettingsBuffer{"OutSkyboxSettingsBuffer"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk::raytraced_shadow_view
{
    constexpr string_view InCameraBuffer{"CameraBuffer"};
    constexpr string_view InLightBuffer{"LightBuffer"};
    constexpr string_view InMeanFilterConfig{"MeanFilterConfig"};
    constexpr string_view InResolution{"Resolution"};
    constexpr string_view InConfig{"Config"};
    constexpr string_view InDepthBuffer{"DepthBuffer"};
    constexpr string_view InVisibilityBuffer{"VisibilityBuffer"};
    constexpr string_view InMeshDatabase{"MeshDatabase"};
    constexpr string_view InInstanceTables{"InstanceTables"};
    constexpr string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr string_view InDisocclusionMask{"DisocclusionMask"};
    constexpr string_view InMotionVectors{"MotionVectors"};

    constexpr string_view OutShadow{"Shadow"};
    constexpr string_view OutShadowSink{"ShadowSink"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk::surfels_gi
{
    constexpr string_view InCameraDataSink{"CameraDataSink"};
    constexpr string_view InGridBounds{"GridBounds"};
    constexpr string_view InGridCellSize{"GridCellSize"};
    constexpr string_view InMaxSurfels{"MaxSurfels"};
    constexpr string_view InMaxRayPaths{"MaxRayPaths"};
    constexpr string_view InGIMultiplier{"GIMultiplier"};
    constexpr string_view InTileCoverageSink{"TileCoverageSink"};
    constexpr string_view InEcsEntitySetBuffer{"EcsEntitySetBuffer"};
    constexpr string_view InMeshDatabase{"MeshDatabase"};
    constexpr string_view InInstanceTables{"InstanceTables"};
    constexpr string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr string_view InLightBuffer{"LightDataBuffer"};
    constexpr string_view InLightConfig{"LightConfig"};
    constexpr string_view InSkyboxSettingsBuffer{"SkyboxSettingsBuffer"};

    constexpr string_view OutLastFrameGrid{"LastGrid"};
    constexpr string_view OutLastFrameGridData{"LastGridData"};
    constexpr string_view OutLastFrameSurfelData{"LastSurfelData"};
    constexpr string_view OutLastFrameSurfelSpawnData{"LastSurfelSpawnData"};
    constexpr string_view OutLastFrameSurfelsLightingData{"LastFrameSurfelsLightingData"};
    constexpr string_view OutUpdatedSurfelGrid{"UpdatedSurfelGrid"};
    constexpr string_view OutUpdatedSurfelGridData{"UpdatedSurfelGridData"};
    constexpr string_view OutUpdatedSurfelData{"UpdatedSurfelData"};
    constexpr string_view OutUpdatedSurfelLightingData{"UpdatedSurfelLightingData"};
    constexpr string_view OutUpdatedSurfelLightEstimatorData{"UpdatedSurfelLightEstimatorData"};
    constexpr string_view OutSurfelsLastUsage{"SurfelsLastUsage"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk::swapchain_graph
{
    constexpr string_view InAcquiredImage{"AcquiredImage"};
    constexpr string_view OutAcquiredImage{"AcquiredImage"};
    constexpr string_view InRenderedImage{"ImageToPresent"};
    constexpr string_view OutPresentedImage{"PresentedImage"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry();
}