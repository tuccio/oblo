#pragma once

#include <oblo/asset/import/file_importer.hpp>
#include <oblo/core/unique_ptr.hpp>

namespace oblo::importers
{
    class gltf final : public file_importer
    {
    public:
        gltf();
        gltf(const gltf&) = delete;
        gltf(gltf&&) noexcept = delete;
        gltf& operator=(const gltf&) = delete;
        gltf& operator=(gltf&&) noexcept = delete;
        ~gltf();

        bool init(const import_config& config, import_preview& preview);
        bool import(import_context context);
        file_import_results get_results();

    private:
        struct impl;

    private:
        unique_ptr<impl> m_impl;
    };
}