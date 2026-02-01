#pragma once

#if defined(OBLO_CODEGEN)
    #define OBLO_ANNOTATE(Annotation...) __attribute__((annotate(#Annotation)))
#else
    #define OBLO_ANNOTATE(...)
#endif

#define OBLO_REFLECT(...) OBLO_ANNOTATE(_oblo_reflect, __VA_ARGS__)
#define OBLO_PROPERTY(...) OBLO_REFLECT(__VA_ARGS__)
#define OBLO_ENUM(...) OBLO_REFLECT(__VA_ARGS__)
#define OBLO_COMPONENT(Uuid, ...) OBLO_REFLECT(Component, UUID=Uuid, __VA_ARGS__)
#define OBLO_TAG(Uuid, ...) OBLO_REFLECT(Tag, UUID=Uuid, __VA_ARGS__)
#define OBLO_RESOURCE(...) OBLO_REFLECT(Resource, __VA_ARGS__)