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

    constexpr std::string_view InLightData{"LightDataBuffer"};
    constexpr std::string_view InLightConfig{"LightConfig"};

    struct config
    {
        bool withPicking{false};
    };

    frame_graph_template create(const frame_graph_registry& registry, const config& cfg = {});
}

namespace oblo::vk::scene_data
{
    constexpr std::string_view InLightData{"LightData"};
    constexpr std::string_view OutLightData{"LightDataBuffer"};
    constexpr std::string_view OutLightConfig{"LightConfig"};
    constexpr std::string_view OutInstanceTables{"InstanceTables"};
    constexpr std::string_view OutInstanceBuffers{"InstanceBuffers"};

    frame_graph_template create(const frame_graph_registry& registry);
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry();
}