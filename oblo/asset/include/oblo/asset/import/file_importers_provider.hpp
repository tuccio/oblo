#pragma once

#include <oblo/core/dynamic_array.hpp>

namespace oblo
{
    struct file_importer_descriptor;

    class file_importers_provider
    {
    public:
        virtual ~file_importers_provider() = default;

        virtual void fetch_importers(dynamic_array<file_importer_descriptor>& outImporters) const = 0;
    };
}