#ifndef OBLO_INCLUDE_RENDERER_TRANSFORM
#define OBLO_INCLUDE_RENDERER_TRANSFORM

struct transform
{
    mat4 localToWorld;
    mat4 lastFrameLocalToWorld;
    mat4 normalMatrix;
};

layout(buffer_reference) buffer i_TransformBufferType
{
    transform values[];
};

#endif