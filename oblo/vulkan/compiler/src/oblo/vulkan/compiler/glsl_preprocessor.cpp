#include <oblo/vulkan/compiler/glsl_preprocessor.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/filesystem/file.hpp>
#include <oblo/core/filesystem/filesystem.hpp>

namespace oblo::vk
{
    namespace
    {
        void prepare_for_line_directive(string_builder& out, cstring_view path)
        {
            if (!filesystem::absolute(path, out))
            {
                out = path;
            }

            // A little hacky, we use / instead of \ on windows to make sure we don't have escapes in our strings
            for (auto& c : out.mutable_data())
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }
        }
    }
    struct glsl_preprocessor::source_file
    {
        struct include
        {
            usize directiveBegin;
            usize pathBegin;
            usize pathEnd;
            u32 line;
            source_file* file;
        };

        explicit source_file(allocator* allocator) : content{allocator}, includes{allocator} {}

        string_builder content;
        string_builder fullPath;
        deque<include> includes;
        usize versionDirectiveLength{0};
    };

    glsl_preprocessor::glsl_preprocessor(allocator& allocator) :
        m_memoryResource{&allocator}, m_builder{&allocator}, m_sourceFiles{&allocator},
        m_sourceFilesMap{&m_memoryResource}, m_includesMap{&m_memoryResource}
    {
    }

    void glsl_preprocessor::set_emit_line_directives(bool emitLineDirectives)
    {
        m_emitLineDirectives = emitLineDirectives;
    }

    bool glsl_preprocessor::process_from_file(
        cstring_view path, string_view preamble, resolve_include_fn resolveInclude)
    {
        OBLO_ASSERT(m_sourceFilesMap.empty());

        m_builder.clear().append(path);
        auto* const mainSource = add_or_get_file(m_builder);

        if (!mainSource)
        {
            return false;
        }

        deque<source_file*> files{m_memoryResource.get_allocator()};
        files.push_back(mainSource);

        while (!files.empty())
        {
            u32 lineCount = 0;

            source_file* file = files.back();
            files.pop_back();

            const auto content = string_view{file->content};

            auto* charIt = content.begin();

            if (content.starts_with("#version"))
            {
                while (charIt != content.end() && *charIt != '\n')
                {
                    ++charIt;
                }

                // +1 to include the newline
                const u32 maybePlus1 = u32(charIt != content.end());
                file->versionDirectiveLength = usize(maybePlus1 + charIt - content.begin());
            }

            // We need to find each #include <...> and track the position in the string in the includes of source_file

            for (; charIt != content.end();)
            {
                if (*charIt == '#')
                {
                    const string_view directive = content.substr(charIt - content.begin());

                    auto* const directiveLineBegin = charIt;

                    if (directive.starts_with("#include"))
                    {
                        // Find the <
                        while (charIt != content.end() && *charIt != '\n' && *charIt != '<')
                        {
                            ++charIt;
                        }

                        if (charIt != content.end() && *charIt == '<')
                        {
                            auto* const includePathBegin = ++charIt;

                            // Find the >
                            while (charIt != content.end() && *charIt != '\n' && *charIt != '>')
                            {
                                ++charIt;
                            }

                            if (charIt != content.end() && *charIt == '>')
                            {
                                auto* const includeEnd = charIt;

                                file->includes.push_back({
                                    .directiveBegin = usize(directiveLineBegin - content.begin()),
                                    .pathBegin = usize(includePathBegin - content.begin()),
                                    .pathEnd = usize(includeEnd - content.begin()),
                                    .line = lineCount,
                                });
                            }
                        }
                    }
                }

                // Go to the next line
                while (charIt != content.end() && *charIt != '\n')
                {
                    lineCount += u32{*charIt == '\n'};
                    ++charIt;
                }

                // Skip all whitespaces
                while (charIt != content.end() && std::isspace(*charIt))
                {
                    lineCount += u32{*charIt == '\n'};
                    ++charIt;
                }
            }

            // After parsing, look at the other includes

            for (auto& include : file->includes)
            {
                // Check if we already found the include
                const auto includePath = content.substr(include.pathBegin, include.pathEnd - include.pathBegin);

                const auto includeIt = m_includesMap.find(includePath);

                if (includeIt != m_includesMap.end())
                {
                    include.file = includeIt->second;
                }
                else
                {
                    // Otherwise search it using the search callback
                    m_builder.clear();

                    if (!resolveInclude(includePath, m_builder))
                    {
                        m_hasError = true;
                        m_builder.clear().format("Failed to resolve include: {}", includePath);
                        return false;
                    }

                    auto* const includeSourceFile = add_or_get_file(m_builder);

                    if (!includeSourceFile)
                    {
                        return false;
                    }

                    // Add it to the includes map, in case we find the same include again
                    m_includesMap.emplace(includePath, includeSourceFile);

                    include.file = includeSourceFile;

                    // Add it to the queue for parsing
                    files.push_back(includeSourceFile);
                }
            }
        }

        m_hasError = false;

        string_builder lineDirectiveBuilder{m_memoryResource.get_allocator()};

        const auto expandIncludes = [this, &lineDirectiveBuilder](const source_file* file,
                                        function_ref<void(string_view)> append,
                                        auto&& recursive) -> void
        {
            const auto content = string_view{file->content};

            usize currentOffset = file->versionDirectiveLength;

            for (auto& include : file->includes)
            {
                // Copy all the code that preceeds the include
                const auto data = content.substr(currentOffset, include.directiveBegin - currentOffset);
                append(data);

                currentOffset += data.size();

                // Add the #line directive
                const auto includePath = string_view{include.file->fullPath};

                if (m_emitLineDirectives)
                {
                    append("#line 1 \"");
                    append(includePath);
                    append("\"\n");
                }

                // Expand the include recursively
                recursive(include.file, append, recursive);

                if (m_emitLineDirectives)
                {
                    // We add +1 because the errors seem to be 1 line off otherwise
                    lineDirectiveBuilder.clear().format("{}", 1 + include.line);
                    append("\n#line ");
                    append(lineDirectiveBuilder.as<string_view>());

                    append(" \"");
                    append(file->fullPath);
                    append("\"\n");
                }

                // Skip the include directive and move forward without copying
                // +1 to skip the > as well
                currentOffset += 1 + include.pathEnd - include.directiveBegin;
            }

            // Finally copy the rest, includes are done
            const auto data = content.substr(currentOffset);
            append(data);
        };

        const auto buildAll = [&](function_ref<void(string_view)> append)
        {
            // We need to start with the #version directive
            const auto versionDirective =
                mainSource->content.as<string_view>().substr(0, mainSource->versionDirectiveLength);

            append(versionDirective);

            // Now we can append our custom preamble (e.g global defines)
            append(preamble);

            // Finally expand the includes
            expandIncludes(mainSource, append, expandIncludes);
        };

        // Calculate the size first
        usize codeLength = 0;
        buildAll([&codeLength](string_view str) { codeLength += str.size(); });

        // Then write the processed source code
        m_builder.clear();
        m_builder.reserve(codeLength);

        buildAll([this](string_view str) { m_builder.append(str); });

        OBLO_ASSERT(m_builder.size() == codeLength);

        return true;
    }

    glsl_preprocessor::source_file* glsl_preprocessor::add_or_get_file(const string_builder& path)
    {
        if (const auto it = m_sourceFilesMap.find(path); it != m_sourceFilesMap.end())
        {
            return it->second;
        }

        const auto [it, inserted] =
            m_sourceFilesMap.emplace(string_builder{m_memoryResource.get_allocator(), path.as<string_view>()}, nullptr);

        OBLO_ASSERT(inserted);

        auto& sourceFile = m_sourceFiles.emplace_back(m_memoryResource.get_allocator());

        const auto r = filesystem::load_text_file_into_memory(sourceFile.content, it->first);

        if (!r)
        {
            m_hasError = true;
            m_builder.clear().format("Failed to read file: {}", it->first);
            return nullptr;
        }

        prepare_for_line_directive(sourceFile.fullPath, it->first);

        it->second = &sourceFile;

        return &sourceFile;
    }

    glsl_preprocessor::~glsl_preprocessor() = default;

    cstring_view glsl_preprocessor::get_code() const
    {
        OBLO_ASSERT(!m_hasError);
        return m_builder;
    }

    cstring_view glsl_preprocessor::get_error() const
    {
        OBLO_ASSERT(m_hasError);
        return m_builder;
    }

    void glsl_preprocessor::get_source_files(deque<string_view>& sourceFiles) const
    {
        for (const auto& [p, f] : m_sourceFilesMap)
        {
            sourceFiles.emplace_back(p);
        }
    }
}