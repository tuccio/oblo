#pragma once

#ifdef OBLO_ENABLE_ASSERT

    #define OBLO_ASSERT_2(Condition, Message)                                                                          \
        {                                                                                                              \
            if (!(Condition))                                                                                          \
                oblo::debug_assert_report(__FILE__, __LINE__, Message);                                                \
        };

    #define OBLO_ASSERT_1(Condition) OBLO_ASSERT_2((Condition), #Condition)
    #define OBLO_ASSERT_OVERLOAD(_1, _2, NAME, ...) NAME
    #define OBLO_ASSERT(...) OBLO_ASSERT_OVERLOAD(__VA_ARGS__, OBLO_ASSERT_2, OBLO_ASSERT_1)(__VA_ARGS__)

#else

    #define OBLO_ASSERT(...)

#endif

namespace oblo
{
    void debug_assert_report(const char* filename, int lineNumber, const char* message);
}

#if defined(_MSC_VER)
    #define OBLO_DEBUGBREAK() __debugbreak()
#else
    #define OBLO_DEBUGBREAK() asm("int $3")
#endif