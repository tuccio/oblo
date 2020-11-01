#pragma once

#ifdef OBLO_ENABLE_ASSERT

#define OBLO_ASSERT_2(Condition, Message)
#define OBLO_ASSERT_1(Condition) OBLO_ASSERT_2(Condition, #Condition)
#define OBLO_ASSERT(...) OBLO_ASSERT_OVERLOAD(__VA_ARGS__, OBLO_ASSERT_2, OBLO_ASSERT_1)(__VA_ARGS__)

#else

#define OBLO_ASSERT(...)

#endif

namespace oblo
{
    void debug_assert_report();
}

#define OBLO_DEBUGBREAK() asm("int $3")