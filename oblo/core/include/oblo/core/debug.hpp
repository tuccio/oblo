#pragma once

#ifdef OBLO_ENABLE_ASSERT

namespace oblo::detail
{
    namespace
    {
        template <int line>
        bool assert_triggered = false;
    }
}

    #define OBLO_ASSERT_2(Condition, Message)                                                                          \
        {                                                                                                              \
            if (!(Condition))                                                                                          \
            {                                                                                                          \
                oblo::debug_assert(__FILE__, __LINE__, Message);                                                       \
            }                                                                                                          \
        }

    #define OBLO_ASSERT_1(Condition) OBLO_ASSERT_2((Condition), #Condition)
    #define OBLO_ASSERT_OVERLOAD(_1, _2, NAME, ...) NAME
    #define OBLO_ASSERT(...) OBLO_ASSERT_OVERLOAD(__VA_ARGS__, OBLO_ASSERT_2, OBLO_ASSERT_1)(__VA_ARGS__)

    #define OBLO_ASSERT_ONCE(...)                                                                                      \
        if (!oblo::detail::assert_triggered<__LINE__>)                                                                 \
        {                                                                                                              \
            OBLO_ASSERT(__VA_ARGS__);                                                                                  \
            oblo::detail::assert_triggered<__LINE__> = true;                                                           \
        }

#else

    #define OBLO_ASSERT(...)
    #define OBLO_ASSERT_ONCE(...)

#endif

namespace oblo
{
    void debug_assert_hook_install();
    void debug_assert_hook_remove();

    void debug_assert_report(const char* filename, int lineNumber, const char* message);

    // Used to throw in OBLO_ASSERT, to make the assertion compatible with constexpr
    // Ideally exception would be replaced with if consteval eventually
    struct debug_assert
    {
        debug_assert(const char* filename, int lineNumber, const char* message)
        {
            debug_assert_report(filename, lineNumber, message);
        }
    };

#ifdef OBLO_ENABLE_ASSERT
    constexpr bool assert_enabled = true;
#else
    constexpr bool assert_enabled = false;
#endif
}

#if defined(__clang__)
    #define OBLO_DEBUGBREAK() __builtin_debugtrap()
#elif defined(_MSC_VER)
    #define OBLO_DEBUGBREAK() __debugbreak()
#endif