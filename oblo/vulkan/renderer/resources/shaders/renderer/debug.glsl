#ifndef OBLO_INCLUDE_RENDERER_DEBUG
#define OBLO_INCLUDE_RENDERER_DEBUG

vec3 debug_color_map(uint value)
{
    vec3 colors[20];
    colors[0] = vec3(1.0, 0.0, 0.0);     // Red
    colors[1] = vec3(0.0, 1.0, 0.0);     // Green
    colors[2] = vec3(0.0, 0.0, 1.0);     // Blue
    colors[3] = vec3(1.0, 1.0, 0.0);     // Yellow
    colors[4] = vec3(1.0, 0.0, 1.0);     // Magenta
    colors[5] = vec3(0.0, 1.0, 1.0);     // Cyan
    colors[6] = vec3(1.0, 0.5, 0.0);     // Orange
    colors[7] = vec3(0.5, 0.0, 1.0);     // Purple
    colors[8] = vec3(0.5, 0.5, 0.5);     // Grey
    colors[9] = vec3(1.0, 0.75, 0.8);    // Pink
    colors[10] = vec3(0.0, 0.5, 0.0);    // Dark Green
    colors[11] = vec3(0.5, 0.0, 0.0);    // Dark Red
    colors[12] = vec3(0.0, 0.0, 0.5);    // Dark Blue
    colors[13] = vec3(0.5, 0.5, 0.0);    // Olive
    colors[14] = vec3(0.5, 0.0, 0.5);    // Maroon
    colors[15] = vec3(0.0, 0.5, 0.5);    // Teal
    colors[16] = vec3(0.75, 0.75, 0.75); // Light Grey
    colors[17] = vec3(0.5, 0.75, 0.0);   // Lime
    colors[18] = vec3(0.75, 0.5, 0.0);   // Gold
    colors[19] = vec3(0.5, 0.25, 0.0);   // Brown

    // Ensure the value is within the range of defined colors
    uint index = value % 10;

    return colors[index];
}

#endif