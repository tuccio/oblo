#pragma once

#include <oblo/ecs/systems/system_graph_usages.hpp>

namespace oblo::system_graph_usages
{
    constexpr ecs::system_graph_usage editor = "editor"_hsv;
    constexpr ecs::system_graph_usage scripts = "scripts"_hsv;
}