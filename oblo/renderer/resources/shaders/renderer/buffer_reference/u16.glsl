#ifndef OBLO_INCLUDE_RENDERER_BUFFER_REFERENCE_U16
#define OBLO_INCLUDE_RENDERER_BUFFER_REFERENCE_U16

// #extension GL_EXT_buffer_reference : require
// #extension GL_EXT_shader_16bit_storage : require

layout(buffer_reference) buffer U16AttributeType
{
    uint16_t values[];
};

#endif