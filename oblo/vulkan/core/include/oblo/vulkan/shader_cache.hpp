#pragma once

#include <oblo/core/string/string_view.hpp>
#include <oblo/core/types.hpp>

#include <vulkan/vulkan.h>

#include <filesystem>
#include <span>

namespace oblo
{
    class frame_allocator;
}

namespace oblo::vk
{
    namespace shader_compiler
    {
        struct options;
    }

    class shader_cache
    {
    public:
        bool init(const std::filesystem::path& dir);

        bool find_or_add(std::span<unsigned>& outSpirv,
            frame_allocator& allocator,
            string_view debugName,
            string_view sourceCode,
            VkShaderStageFlagBits stage,
            const shader_compiler::options& options);

    private:
        std::filesystem::path m_path;
    };
}