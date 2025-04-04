#pragma once

#if defined(OBLO_CODEGEN)
    #define OBLO_ANNOTATE(Annotation...) __attribute__((annotate(#Annotation)))
#else
    #define OBLO_ANNOTATE(...)
#endif

#define OBLO_REFLECT(...) OBLO_ANNOTATE(_oblo_reflect, __VA_ARGS__)
#define OBLO_COMPONENT(...) OBLO_REFLECT(Component, __VA_ARGS__)