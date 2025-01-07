#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/types.hpp>

#include <span>

namespace oblo
{
    enum class property_kind : u8;

    class data_document;
    class hashed_string_view;
    struct property;
    class property_value_wrapper;

    void property_value_apply(property_kind kind, void* dst, const property_value_wrapper& wrapper);
    void property_value_fetch(property_kind kind, const void* src, property_value_wrapper& wrapper);
}