#include <oblo/properties/property_value_wrapper.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/properties/property_kind.hpp>

namespace oblo
{
    bool property_value_wrapper::assign_to(property_kind k, void* dst) const
    {
        if (m_kind != k)
        {
            return false;
        }

        switch (k)
        {
        case property_kind::string:
            *reinterpret_cast<string*>(dst) = m_str;
            break;

        case property_kind::u8:
            new (dst) u32{m_u8};
            break;

        case property_kind::u16:
            new (dst) u32{m_u16};
            break;

        case property_kind::h32:
            [[fallthrough]];
        case property_kind::u32:
            new (dst) u32{m_u32};
            break;

        case property_kind::h64:
            [[fallthrough]];
        case property_kind::u64:
            new (dst) u64{m_u64};
            break;

        case property_kind::i8:
            new (dst) i8{m_i8};
            break;

        case property_kind::i16:
            new (dst) i16{m_i16};
            break;

        case property_kind::i32:
            new (dst) i32{m_i32};
            break;

        case property_kind::i64:
            new (dst) i64{m_i64};
            break;

        case property_kind::f32:
            new (dst) f32{m_f32};
            break;

        case property_kind::f64:
            new (dst) f64{m_f64};
            break;

        case property_kind::boolean:
            new (dst) bool{m_bool};
            break;

        case property_kind::uuid:
            new (dst) uuid{m_uuid};
            break;

        default:
            OBLO_ASSERT(false);
            return false;
        }

        return true;
    }

    bool property_value_wrapper::assign_from(property_kind k, const void* src)
    {
        switch (k)
        {
        case property_kind::string:

            *this = property_value_wrapper{*reinterpret_cast<const string*>(src)};
            break;

        case property_kind::u8:
            *this = property_value_wrapper{*reinterpret_cast<const u8*>(src)};
            break;

        case property_kind::u16:
            *this = property_value_wrapper{*reinterpret_cast<const u16*>(src)};
            break;

        case property_kind::u32:
            *this = property_value_wrapper{*reinterpret_cast<const u32*>(src)};
            break;

        case property_kind::u64:
            *this = property_value_wrapper{*reinterpret_cast<const u64*>(src)};
            break;

        case property_kind::i8:
            *this = property_value_wrapper{*reinterpret_cast<const i8*>(src)};
            break;

        case property_kind::i16:
            *this = property_value_wrapper{*reinterpret_cast<const i16*>(src)};
            break;

        case property_kind::i32:
            *this = property_value_wrapper{*reinterpret_cast<const i32*>(src)};
            break;

        case property_kind::i64:
            *this = property_value_wrapper{*reinterpret_cast<const i64*>(src)};
            break;

        case property_kind::boolean:
            *this = property_value_wrapper{*reinterpret_cast<const bool*>(src)};
            break;

        case property_kind::uuid:
            *this = property_value_wrapper{*reinterpret_cast<const uuid*>(src)};
            break;

        default:
            OBLO_ASSERT(false);
            m_kind = property_kind::enum_max;
            return false;
        }

        return true;
    }

    std::span<const byte> property_value_wrapper::get_bytes() const noexcept
    {
        return {data(), size()};
    }

    const byte* property_value_wrapper::data() const noexcept
    {
        return std::launder(m_bytes);
    }

    usize property_value_wrapper::size() const noexcept
    {
        return get_size_and_alignment(m_kind).first;
    }
}