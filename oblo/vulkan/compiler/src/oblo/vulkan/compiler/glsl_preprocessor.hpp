#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/stl/memory_resource_adapter.hpp>
#include <oblo/core/string/string_builder.hpp>
#include <oblo/core/string/transparent_string_hash.hpp>

#include <span>
#include <unordered_map>

namespace oblo::vk
{
    using resolve_include_fn = function_ref<bool(string_view include, string_builder& out)>;

    class glsl_preprocessor
    {
    public:
        struct source_file;

    public:
        glsl_preprocessor() = delete;
        explicit glsl_preprocessor(allocator& allocator);
        glsl_preprocessor(const glsl_preprocessor&) = delete;
        glsl_preprocessor(glsl_preprocessor&&) noexcept = delete;
        ~glsl_preprocessor();

        glsl_preprocessor& operator=(const glsl_preprocessor&) = delete;
        glsl_preprocessor& operator=(glsl_preprocessor&&) noexcept = delete;

        // We may want to avoid emitting line directives not to confuse tooling in certain cases
        void set_emit_line_directives(bool emitLineDirectives);

        bool process_from_file(cstring_view path, string_view preamble, resolve_include_fn searchInclude);

        cstring_view get_code() const;

        cstring_view get_error() const;

        void get_source_files(deque<string_view>& sourceFiles) const;

        auto& get_source_files_map() const
        {
            return m_sourceFilesMap;
        }

    private:
        source_file* add_or_get_file(const string_builder& path);

    private:
        bool m_hasError{};
        bool m_emitLineDirectives{true};
        memory_resource_adapter m_memoryResource;
        string_builder m_builder;
        deque<source_file> m_sourceFiles;
        std::pmr::unordered_map<string_builder, source_file*, transparent_string_hash> m_sourceFilesMap;
        std::pmr::unordered_map<string_view, source_file*, transparent_string_hash> m_includesMap;
    };
}