#pragma once

#include <oblo/core/handle.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

namespace oblo
{
    class string_interner;
    struct string;
}

namespace oblo::vk
{
    class allocator;
    class resource_manager;
    struct buffer;

    struct buffer_column_description
    {
        h32<string> name;
        u32 elementSize;
    };

    class buffer_table
    {
    public:
        using column_description = buffer_column_description;

    public:
        buffer_table();
        buffer_table(const buffer_table&) = delete;
        buffer_table(buffer_table&&) noexcept = delete;
        buffer_table& operator=(const buffer_table&) = delete;
        buffer_table& operator=(buffer_table&&) noexcept = delete;
        ~buffer_table();

        [[nodiscard]] u32 init(const buffer& buffer,
            std::span<const buffer_column_description> columns,
            resource_manager& resourceManager,
            u32 rows,
            u32 bufferAlignment);

        void shutdown(resource_manager& resourceManager);

        std::span<const h32<string>> names() const;
        std::span<const h32<buffer>> buffers() const;
        std::span<const u32> element_sizes() const;

        i32 find(h32<string> name) const;

        u32 rows_count() const;
        u32 columns_count() const;

    private:
        i32* m_stringToBufferIndexMap{nullptr};
        h32<buffer>* m_buffers{nullptr};
        h32<string>* m_names{nullptr};
        u32* m_elementSizes{nullptr};
        u32 m_rows{0u};
        u32 m_columns{0u};
        u32 m_stringRangeMin{0u};
        u32 m_stringRangeMax{0u};
    };
}