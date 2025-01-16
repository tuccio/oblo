#ifndef OBLO_INCLUDE_RENDERER_DEBUG
#define OBLO_INCLUDE_RENDERER_DEBUG

vec3 debug_color_map(uint value)
{
    vec3 colors[13];
    colors[0] = vec3(0.388, 0.431, 0.980); // Blue (#636EFA)
    colors[1] = vec3(0.937, 0.329, 0.231); // Red (#EF553B)
    colors[2] = vec3(0.000, 0.800, 0.588); // Green (#00CC96)
    colors[3] = vec3(0.671, 0.388, 0.980); // Purple (#AB63FA)
    colors[4] = vec3(1.000, 0.631, 0.353); // Orange (#FFA15A)
    colors[5] = vec3(0.098, 0.827, 0.953); // Cyan (#19D3F3)
    colors[6] = vec3(1.000, 0.400, 0.572); // Pink (#FF6692)
    colors[7] = vec3(0.714, 0.910, 0.502); // Light Green (#B6E880)
    colors[8] = vec3(1.000, 0.592, 1.000); // Light Pink (#FF97FF)
    colors[9] = vec3(0.996, 0.796, 0.322); // Yellow (#FECB52)
    colors[10] = vec3(0.122, 0.467, 0.706); // Dark Blue (#1F77B4)
    colors[11] = vec3(0.090, 0.741, 0.753); // Teal (#17BECF)
    colors[12] = vec3(0.616, 0.000, 1.000); // Violet (#9D00FF)

    // Ensure the value is within the range of defined colors
    uint index = value % 13;

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