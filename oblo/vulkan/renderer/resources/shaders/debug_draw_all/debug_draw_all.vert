#version 450

layout(location = 0) in vec3 in_Position;

void main()
{
#if 0
    const vec2 data[3] = {
        vec2(-.5f, .5f),
        vec2(.5f, .5f),
        vec2(0.f, -.5f),
    };
    
    const vec2 uv = data[gl_VertexIndex % 3];

    const vec3 position = vec3(uv.xy, 0.f);
#else
    vec3 position = in_Position;

    position.z /= 100.f;
    position.z += 0.2f;
#endif

    gl_Position = vec4(position, 1);
}