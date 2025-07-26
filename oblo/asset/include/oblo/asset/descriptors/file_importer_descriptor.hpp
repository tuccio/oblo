#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/unique_ptr.hpp>

#include <span>

namespace oblo
{
    class any;
    class file_importer;

    using create_file_importer_fn = unique_ptr<file_importer> (*)(const any& userdata);

    struct file_importer_descriptor
    {
        type_id type;
        create_file_importer_fn create;
        std::span<const string_view> extensions;
    };
}