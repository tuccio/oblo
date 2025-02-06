#pragma once

#include <oblo/asset/import/file_importer.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo::importers
{
    class assimp : public file_importer
    {
    public:
        assimp();
        assimp(const assimp&) = delete;
        assimp(assimp&&) noexcept = delete;
        ~assimp();

        assimp& operator=(const assimp&) = delete;
        assimp& operator=(assimp&&) noexcept = delete;

        bool init(const import_config& config, import_preview& preview);
        bool import(import_context context);
        file_import_results get_results();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}