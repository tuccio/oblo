#pragma once

#include <oblo/vulkan/graph/frame_graph_template.hpp>

#include <string_view>

namespace oblo::vk::main_view
{
    constexpr std::string_view InPickingConfiguration{"Picking Configuration"};
    constexpr std::string_view InResolution{"Resolution"};
    constexpr std::string_view InCamera{"Camera"};
    constexpr std::string_view InTime{"Time"};
    constexpr std::string_view InFinalRenderTarget{"Final Render Target"};

    struct config
    {
        bool bypassCulling{false};
        bool withPicking{false};
    };

    frame_graph_template create(const frame_graph_registry& registry, const config& cfg = {});
}

namespace oblo::vk
{
    vk::frame_graph_registry create_frame_graph_registry();
}