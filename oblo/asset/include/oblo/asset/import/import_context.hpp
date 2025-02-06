#pragma once

#include <oblo/asset/import/import_preview.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/types.hpp>
#include <oblo/core/uuid.hpp>

#include <span>

namespace oblo
{
    class data_document;
    class importer;
    class string_builder;
    struct import_context_impl;

    class import_context
    {
    public:
        cstring_view get_output_path(const uuid& id, string_builder& outPath, string_view optExtension = {}) const;

        std::span<const import_node> get_import_nodes() const;
        std::span<const import_node> get_child_import_nodes(usize i) const;
        std::span<const import_node_config> get_import_node_configs() const;
        std::span<const import_node_config> get_child_import_node_configs(usize index) const;

        const data_document& get_settings() const;

    private:
        friend class importer;

    private:
        const import_context_impl* m_impl;
    };
}