#include <oblo/properties/property_access.hpp>

#include <oblo/core/string/string.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_tree.hpp>

namespace oblo
{
   

    // void property_value_apply(property_kind kind, void* dst, const void* value)
    //{
    //     switch (kind)
    //     {
    //     case property_kind::string:
    //         *reinterpret_cast<string*>(dst) = *reinterpret_cast<const string*>(value);
    //         break;

    //    default:
    //        std::memcpy(dst, value, get_size_and_alignment(kind).first);
    //        break;
    //    }
    //}

    // void property_value_fetch(property_kind kind, const void* src, void* value)
    //{
    //     switch (kind)
    //     {
    //     case property_kind::string:
    //         *reinterpret_cast<string*>(value) = *reinterpret_cast<const string*>(src);
    //         break;

    //    default:
    //        std::memcpy(value, src, get_size_and_alignment(kind).first);
    //        break;
    //    }
    //}

    // bool property_apply(const property& p, std::span<const usize> arrayIndices, void* root, const void* value)
    //{
    //     // TODO: If it's a meta property, do something else
    //     // switch (p.meta)
    //     //{
    //     // case meta_properties::kind::none:
    //     //    break;
    //     // case meta_properties::kind::array_size:
    //     //    // TODO: Resize array
    //     //    return;
    //     // case meta_properties::kind::array_element:
    //     //    // TODO: Apply to index
    //     //    return;
    //     //}

    //    u8* const propertyPtr = static_cast<u8*>(root) + p.offset;
    //    property_value_apply(p.kind, propertyPtr, value);

    //    return true;
    //}

    // bool property_fetch(const property& p, std::span<const usize> arrayIndices, const void* root, void* value)
    //{
    //     // TODO: If it's a meta property, do something else

    //    const u8* const propertyPtr = static_cast<const u8*>(root) + p.offset;
    //    property_value_fetch(p.kind, propertyPtr, value);

    //    return true;
    //}

    // bool property_apply(const property& p,
    //     std::span<const usize> arrayIndices,
    //     void* dst,
    //     const data_document& src,
    //     u32 valueNode,
    //     hashed_string_view name);

    // bool property_fetch(const property& p,
    //     std::span<const usize> arrayIndices,
    //     const void* src,
    //     data_document& dst,
    //     u32 parent,
    //     hashed_string_view name);
}