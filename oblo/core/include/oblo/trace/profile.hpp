#pragma once

#ifdef TRACY_ENABLE

    #include <oblo/core/string/string_view.hpp>

    #include <source_location>

    #include <tracy/Tracy.hpp>

namespace oblo::trace
{
    consteval const char* make_scope_name(const char* name)
    {
        return name;
    }

    consteval const char* make_scope_name(std::source_location loc = std::source_location::current())
    {
        return loc.function_name();
    }
}

    #define OBLO_PROFILE_FRAME_BEGIN()
    #define OBLO_PROFILE_FRAME_END() FrameMark
    #define OBLO_PROFILE_SCOPE(...) ZoneScopedN(oblo::trace::make_scope_name(__VA_ARGS__))

    #define OBLO_PROFILE_TAG(Text)                                                                                     \
        {                                                                                                              \
            const string_view _trace_tag{Text};                                                                        \
            ZoneText(_trace_tag.data(), _trace_tag.size());                                                            \
        }

#else

    #define OBLO_PROFILE_FRAME_BEGIN()
    #define OBLO_PROFILE_FRAME_END()
    #define OBLO_PROFILE_SCOPE(...)
    #define OBLO_PROFILE_TAG(Text)

#endif