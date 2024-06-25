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

vec3 debug_color_map(float t)
{
    // Clamp t to the range [0, 1]
    t = clamp(t, 0.0, 1.0);

    const vec4 kRedVec4 = vec4(0.13013628, 0.01228611, -0.12644665, 0.0);
    const vec4 kGreenVec4 = vec4(0.08473612, -0.36036449, 1.00093811, -0.00623720);
    const vec4 kBlueVec4 = vec4(0.23704089, -0.01417262, 0.11912573, -0.00067326);

    const vec4 kRedCoeffs = vec4(0.00000000, 0.50338712, 0.42356799, 0.19761808);
    const vec4 kGreenCoeffs = vec4(0.00000000, 0.30734529, 0.22668336, 0.33016341);
    const vec4 kBlueCoeffs = vec4(0.00000000, 0.31380194, 0.22656971, 0.27229275);

    vec4 tVec4 = vec4(1.0, t, t * t, t * t * t);

    float r = dot(tVec4, kRedVec4) + dot(tVec4, kRedCoeffs);
    float g = dot(tVec4, kGreenVec4) + dot(tVec4, kGreenCoeffs);
    float b = dot(tVec4, kBlueVec4) + dot(tVec4, kBlueCoeffs);

    return vec3(r, g, b);
}

#endif