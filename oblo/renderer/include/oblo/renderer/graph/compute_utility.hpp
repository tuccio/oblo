#pragma once

#include <oblo/core/utility.hpp>
#include <oblo/math/vec3u.hpp>
#include <oblo/renderer/graph/frame_graph_context.hpp>

namespace oblo
{
    vec3u calculate_group_size_1d(const gpu_info& info, u32 numThreads)
    {
        const u32 totalGroups = round_up_div(numThreads, info.subgroupSize);

        vec3u groups;
        groups.x = min(totalGroups, info.maxGroupsX);
        groups.y = round_up_div(totalGroups, info.maxGroupsX);
        groups.z = 1u;
        return groups;
    }
}