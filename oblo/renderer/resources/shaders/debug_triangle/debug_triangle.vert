#version 450

layout(location = 0) out vec3 out_Color;

void main()
{
    const vec2 data[3] = {
        vec2(-.5f, .5f),
        vec2(.5f, .5f),
        vec2(0.f, -.5f),
    };

    const vec2 uv = data[gl_VertexIndex];

    gl_Position = vec4(uv, 0.f, 1.f);
    out_Color = vec3(uv * 2.f + 1.f, 0);
}