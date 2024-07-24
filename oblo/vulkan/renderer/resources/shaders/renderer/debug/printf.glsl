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
    #define printf_float(Value) debugPrintfEXT("[ %f ]\n", Value);
    #define printf_vec3(Vector) debugPrintfEXT("[ %f, %f, %f ]\n", Vector.x, Vector.y, Vector.z);

    #ifdef OBLO_PIPELINE_RAYTRACING

        #define debug_if(Condition, Op)                                                                                \
            if (Condition)                                                                                             \
            {                                                                                                          \
                Op;                                                                                                    \
            }

        #define debug_is_center() gl_LaunchIDEXT.xy == (gl_LaunchSizeEXT.xy / 2)

    #endif

#else

    #define printf_block_begin(Condition)
    #define printf_block_end()
    #define printf_text(Message)
    #define printf_float(Value)
    #define printf_vec3(Vector)

    #define debug_if(Condition, Op)
    #define debug_is_center()

#endif

#endif