#ifndef OBLO_INCLUDE_RENDERER_DEBUG_PRINTF
#define OBLO_INCLUDE_RENDERER_DEBUG_PRINTF

#if 0 // To enable the header, this can be included in the shader:

    #define OBLO_DEBUG_PRINTF 1

    #if OBLO_DEBUG_PRINTF
        #extension GL_EXT_debug_printf : enable
    #endif

#endif

#if GL_EXT_debug_printf == 1 && OBLO_DEBUG_PRINTF == 1

    #define printf_block_begin(Condition)                                                                              \
        if (Condition)                                                                                                 \
        {
    #define printf_block_end() }

    #define printf_text(Message) debugPrintfEXT(Message);

    #define printf_float(Label, Value)                                                                                 \
        printf_text(Label);                                                                                            \
        debugPrintfEXT("[ %f ]\n", Value);

    #define printf_vec2(Label, Vector)                                                                                 \
        printf_text(Label);                                                                                            \
        debugPrintfEXT("[ %f, %f ]\n", Vector.x, Vector.y);

    #define printf_vec3(Label, Vector)                                                                                 \
        printf_text(Label);                                                                                            \
        debugPrintfEXT("[ %f, %f, %f ]\n", Vector.x, Vector.y, Vector.z);

    #define printf_vec4(Label, Vector)                                                                                 \
        printf_text(Label);                                                                                            \
        debugPrintfEXT("[ %f, %f, %f, %f ]\n", Vector.x, Vector.y, Vector.z, Vector.w);

    #define debug_if(Condition, Op)                                                                                    \
        if (Condition)                                                                                                 \
        {                                                                                                              \
            Op;                                                                                                        \
        }

    #ifdef OBLO_PIPELINE_RAYTRACING
        #define debug_is_center() gl_LaunchIDEXT.xy == (gl_LaunchSizeEXT.xy / 2)
    #endif

#else

    #define printf_block_begin(Condition)
    #define printf_block_end()
    #define printf_text(Message)
    #define printf_float(Label, Value)
    #define printf_vec2(Label, Vector)
    #define printf_vec3(Label, Vector)
    #define printf_vec4(Label, Vector)

    #define debug_if(Condition, Op)
    #define debug_is_center()

#endif

#endif