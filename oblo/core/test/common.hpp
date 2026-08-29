#pragma once

#include <oblo/core/types.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo
{
    struct aligned32_value
    {
        alignas(32) u32 values[16];
    };

    class move_only_int
    {
    public:
        explicit move_only_int(i32 v) : m_value{allocate_unique<i32>(v)} {}

        move_only_int(move_only_int&&) noexcept = default;

        bool operator==(const move_only_int& other) const
        {
            return *m_value == *other.m_value;
        }

    private:
        unique_ptr<i32> m_value;
    };
}