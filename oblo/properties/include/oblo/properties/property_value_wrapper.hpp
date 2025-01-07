#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/properties/property_kind.hpp>

#include <span>

namespace oblo
{
    class property_value_wrapper
    {
    public:
        property_value_wrapper() : m_kind{property_kind::enum_max} {}

        property_value_wrapper(const property_value_wrapper&) = default;
        property_value_wrapper(property_value_wrapper&&) noexcept = default;

        property_value_wrapper& operator=(const property_value_wrapper&) = default;
        property_value_wrapper& operator=(property_value_wrapper&&) noexcept = default;

        explicit property_value_wrapper(bool value) : m_kind{property_kind::boolean}, m_bool{value} {}
        explicit property_value_wrapper(u8 value) : m_kind{property_kind::u8}, m_u8{value} {}
        explicit property_value_wrapper(u16 value) : m_kind{property_kind::u16}, m_u16{value} {}
        explicit property_value_wrapper(u32 value) : m_kind{property_kind::u32}, m_u32{value} {}
        explicit property_value_wrapper(u64 value) : m_kind{property_kind::u64}, m_u64{value} {}
        explicit property_value_wrapper(i8 value) : m_kind{property_kind::i8}, m_i8{value} {}
        explicit property_value_wrapper(i16 value) : m_kind{property_kind::i16}, m_i16{value} {}
        explicit property_value_wrapper(i32 value) : m_kind{property_kind::i32}, m_i32{value} {}
        explicit property_value_wrapper(i64 value) : m_kind{property_kind::i64}, m_i64{value} {}
        explicit property_value_wrapper(f32 value) : m_kind{property_kind::f32}, m_f32{value} {}
        explicit property_value_wrapper(f64 value) : m_kind{property_kind::f64}, m_f64{value} {}
        explicit property_value_wrapper(uuid value) : m_kind{property_kind::uuid}, m_uuid{value} {}
        explicit property_value_wrapper(string_view value) : m_kind{property_kind::string}, m_str{value} {}

        bool assign_to(property_kind k, void* dst) const;
        bool assign_from(property_kind k, const void* src);

        property_kind get_kind() const noexcept
        {
            return m_kind;
        }

        u8 get_u8() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::u8);
            return m_u8;
        }

        u16 get_u16() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::u16);
            return m_u16;
        }

        u32 get_u32() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::u32);
            return m_u32;
        }

        u64 get_u64() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::u64);
            return m_u64;
        }

        i8 get_i8() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::i8);
            return m_i8;
        }

        i16 get_i16() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::i16);
            return m_i16;
        }

        i32 get_i32() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::i32);
            return m_i32;
        }

        i64 get_i64() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::i64);
            return m_i64;
        }

        f32 get_f32() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::f32);
            return m_f32;
        }

        f64 get_f64() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::f64);
            return m_f64;
        }

        uuid get_uuid() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::uuid);
            return m_uuid;
        }

        string_view get_string() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::string);
            return m_str;
        }

        bool get_bool() const noexcept
        {
            OBLO_ASSERT(m_kind == property_kind::boolean);
            return m_bool;
        }

        explicit operator bool() const noexcept
        {
            return m_kind != property_kind::enum_max;
        }

    private:
        property_kind m_kind{property_kind::enum_max};

        union {
            bool m_bool;
            u8 m_u8;
            u16 m_u16;
            u32 m_u32;
            u64 m_u64;
            i8 m_i8;
            i16 m_i16;
            i32 m_i32;
            i64 m_i64;
            f32 m_f32;
            f64 m_f64;
            uuid m_uuid;
            string_view m_str;
        };
    };
}