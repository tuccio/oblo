#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/core/uuid.hpp>

#include <span>

namespace oblo
{
    struct import_artifact;
    struct import_config;
    struct import_preview;

    class import_context;
    class string;

    struct file_import_results
    {
        std::span<const import_artifact> artifacts;
        std::span<const string> sourceFiles;
        uuid mainArtifactHint;
    };

    class file_importer
    {
    public:
        virtual ~file_importer() = default;

        virtual bool init(const import_config& config, import_preview& preview) = 0;
        virtual bool import(import_context context) = 0;
        virtual file_import_results get_results() = 0;
    };
}