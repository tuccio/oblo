#pragma once

#include <oblo/vulkan/graph/frame_graph_template.hpp>

#include <string_view>

namespace oblo::vk::main_view
{
    constexpr std::string_view InPickingConfiguration{"Picking Configuration"};
    constexpr std::string_view InResolution{"Resolution"};
    constexpr std::string_view InCamera{"Camera"};
    constexpr std::string_view InTime{"Time"};
    constexpr std::string_view InInstanceTables{"InstanceTables"};
    constexpr std::string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr std::string_view InFinalRenderTarget{"Final Render Target"};

    constexpr std::string_view InLights{"LightData"};
    constexpr std::string_view InLightBuffer{"LightDataBuffer"};
    constexpr std::string_view InLightConfig{"LightConfig"};
    constexpr std::string_view InShadowSink{"ShadowSink"};

    constexpr std::string_view InDebugMode{"Debug Mode"};

    constexpr std::string_view OutResolution{"Resolution"};
    constexpr std::string_view OutCameraBuffer{"Camera Buffer"};
    constexpr std::string_view OutDepthBuffer{"Depth Buffer"};
    constexpr std::string_view OutVisibilityBuffer{"Visibility Buffer"};
    constexpr std::string_view OutLitImage{"Lit Output"};
    constexpr std::string_view OutDebugImage{"Debug Output"};
    constexpr std::string_view OutRTDebugImage{"Debug RT Output"};

    constexpr std::string_view OutPicking{"OutPicking"};

    struct config
    {
        bool withPicking{false};
    };

    frame_graph_template create(const frame_graph_registry& registry, const config& cfg = {});
}

namespace oblo::vk::scene_data
{
    constexpr std::string_view InLights{"LightData"};
    constexpr std::string_view OutLights{"LightData"};
    constexpr std::string_view OutLightBuffer{"LightDataBuffer"};
    constexpr std::string_view OutLightConfig{"LightConfig"};
    constexpr std::string_view OutInstanceTables{"InstanceTables"};
    constexpr std::string_view OutInstanceBuffers{"InstanceBuffers"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk::raytraced_shadow_view
{
    constexpr std::string_view InCameraBuffer{"CameraBuffer"};
    constexpr std::string_view InLightBuffer{"LightBuffer"};
    constexpr std::string_view InResolution{"Resolution"};
    constexpr std::string_view InConfig{"Config"};
    constexpr std::string_view InDepthBuffer{"DepthBuffer"};
    constexpr std::string_view InBlurConfig{"BlurConfig"};
    constexpr std::string_view OutShadow{"Shadow"};
    constexpr std::string_view OutShadowSink{"ShadowSink"};

    frame_graph_template create(const frame_graph_registry& registry);

    type_id get_main_view_barrier_source();
    type_id get_main_view_barrier_target();
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry();
}