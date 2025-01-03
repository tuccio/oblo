#version 460

#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_Color;

layout(location = 0) in vec3 in_SurfelPositionWS;

#include <renderer/debug>
#include <surfels/buffers/surfel_grid_r>

void main()
{
    const surfel_grid_header gridHeader = g_SurfelGridHeader;

    const ivec3 cell = surfel_grid_find_cell(gridHeader, in_SurfelPositionWS);
    const uint cellIndex = surfel_grid_cell_index(gridHeader, cell);

    out_Color = vec4(debug_color_map(cellIndex), 1);
}