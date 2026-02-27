#ifndef OBLO_INCLUDE_RENDERER_BUFFER_REFERENCE_U8
#define OBLO_INCLUDE_RENDERER_BUFFER_REFERENCE_U8

// #extension GL_EXT_buffer_reference : require
// #extension GL_EXT_shader_8bit_storage : require

layout(buffer_reference) buffer U8AttributeType
{
    uint8_t values[];
};

#endif