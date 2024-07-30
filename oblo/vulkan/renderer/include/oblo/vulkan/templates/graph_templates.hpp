#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/vulkan/graph/frame_graph_template.hpp>

namespace oblo::vk::main_view
{
    constexpr string_view InPickingConfiguration{"Picking Configuration"};
    constexpr string_view InResolution{"Resolution"};
    constexpr string_view InCamera{"Camera"};
    constexpr string_view InTime{"Time"};
    constexpr string_view InInstanceTables{"InstanceTables"};
    constexpr string_view InInstanceBuffers{"InstanceBuffers"};
    constexpr string_view InFinalRenderTarget{"Final Render Target"};

    constexpr string_view InLights{"LightData"};
    constexpr string_view InLightBuffer{"LightDataBuffer"};
    constexpr string_view InLightConfig{"LightConfig"};
    constexpr string_view InShadowSink{"ShadowSink"};

    constexpr string_view InDebugMode{"Debug Mode"};

    constexpr string_view OutResolution{"Resolution"};
    constexpr string_view OutCameraBuffer{"Camera Buffer"};
    constexpr string_view OutDepthBuffer{"Depth Buffer"};
    constexpr string_view OutVisibilityBuffer{"Visibility Buffer"};
    constexpr string_view OutLitImage{"Lit Output"};
    constexpr string_view OutDebugImage{"Debug Output"};
    constexpr string_view OutRTDebugImage{"Debug RT Output"};

    constexpr string_view OutPicking{"OutPicking"};

    struct config
    {
        bool withPicking{false};
    };

    frame_graph_template create(const frame_graph_registry& registry, const config& cfg = {});
}

namespace oblo::vk::scene_data
{
    constexpr string_view InLights{"LightData"};
    constexpr string_view OutLights{"LightData"};
    constexpr string_view OutLightBuffer{"LightDataBuffer"};
    constexpr string_view OutLightConfig{"LightConfig"};
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
    constexpr string_view OutShadow{"Shadow"};
    constexpr string_view OutShadowSink{"ShadowSink"};

    frame_graph_template create(const frame_graph_registry& registry);

    type_id get_main_view_barrier_source();
    type_id get_main_view_barrier_target();
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry();
}