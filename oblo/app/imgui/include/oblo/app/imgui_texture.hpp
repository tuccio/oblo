#pragma once

#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/resource/resource_ref.hpp>
#include <oblo/vulkan/graph/forward.hpp>

#include <imgui.h>

namespace oblo
{
    class texture;
}

namespace oblo::imgui
{
    ImTextureID add_image(h32<vk::frame_graph_subgraph> subgraph, string_view output);
    ImTextureID add_image(resource_ref<texture> texture);
}