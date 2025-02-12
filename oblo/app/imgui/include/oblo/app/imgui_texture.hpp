#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/vulkan/graph/forward.hpp>

#include <imgui.h>

namespace oblo::imgui
{
    ImTextureID add_image(h32<vk::frame_graph_subgraph> subgraph, string_view output);
}