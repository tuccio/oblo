#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/invoke/function_ref.hpp>
#include <oblo/core/stl/memory_resource_adapter.hpp>
#include <oblo/core/string/string_builder.hpp>

#include <span>
#include <unordered_map>

namespace oblo::vk
{
    using resolve_include_fn = function_ref<bool(string_view include, string_builder& out)>;

    class glsl_preprocessor
    {
    public:
        glsl_preprocessor() = delete;
        explicit glsl_preprocessor(allocator& allocator);
        glsl_preprocessor(const glsl_preprocessor&) = delete;
        glsl_preprocessor(glsl_preprocessor&&) noexcept = delete;
        ~glsl_preprocessor();

        glsl_preprocessor& operator=(const glsl_preprocessor&) = delete;
        glsl_preprocessor& operator=(glsl_preprocessor&&) noexcept = delete;

        bool process_from_file(string_view path, string_view preamble, resolve_include_fn searchInclude);

        cstring_view get_code() const;

        cstring_view get_error() const;

        void get_source_files(deque<string_view>& sourceFiles) const;

    private:
        struct source_file;

        source_file* add_or_get_file(const string_builder& path);

    private:
        struct transparent_string_hash
        {
            using is_transparent = void;

            usize operator()(string_view str) const
            {
                return hash<string_view>{}(str);
            }

            usize operator()(cstring_view str) const
            {
                return hash<cstring_view>{}(str);
            }

            usize operator()(const string_builder& str) const
            {
                return hash<string_view>{}(str.as<string_view>());
            }
        };

    private:
        bool m_hasError{};
        memory_resource_adapter m_memoryResource;
        string_builder m_builder;
        deque<source_file> m_sourceFiles;
        std::pmr::unordered_map<string_builder, source_file*, transparent_string_hash> m_sourceFilesMap;
        std::pmr::unordered_map<string_view, source_file*, transparent_string_hash> m_includesMap;
    };
}