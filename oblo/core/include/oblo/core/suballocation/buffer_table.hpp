#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>

#include <span>

namespace oblo
{
    struct buffer_table_name;

    struct buffer_table_column_description
    {
        h32<buffer_table_name> name;
        u64 elementSize;
    };

    struct buffer_table_subrange
    {
        u64 begin;
        u64 end;
    };

    /// @brief Allows suballocating a SoA table into an existing array.
    class buffer_table
    {
    public:
        using column_description = buffer_table_column_description;
        using subrange = buffer_table_subrange;

    public:
        buffer_table();
        buffer_table(const buffer_table&) = delete;
        buffer_table(buffer_table&&) noexcept = delete;
        buffer_table& operator=(const buffer_table&) = delete;
        buffer_table& operator=(buffer_table&&) noexcept = delete;
        ~buffer_table();

        /// @brief Initializes the buffer table with the given columns.
        /// @param memStartOffset The starting offset of the memory to sub-allocate, added to all the columns offset.
        /// @param memTotalSize The size of the memory to sub-allocate, for bound checking.
        /// @param columns The columns of the table.
        /// @param rows The number of rows.
        /// @param bufferAlignment The alignment each column sub-buffer requires.
        /// @return The total suballocated size, or an error.
        [[nodiscard]] expected<u64> init(u64 memStartOffset,
            u64 memTotalSize,
            std::span<const buffer_table_column_description> columns,
            u64 rows,
            u64 bufferAlignment);

        void shutdown();

        std::span<const h32<buffer_table_name>> names() const;
        std::span<const subrange> buffer_subranges() const;
        std::span<const u64> element_sizes() const;

        i32 find(h32<buffer_table_name> name) const;

        u64 rows_count() const;
        u64 columns_count() const;

    private:
        i32* m_stringToBufferIndexMap{nullptr};
        subrange* m_bufferSubranges{nullptr};
        h32<buffer_table_name>* m_names{nullptr};
        u64* m_elementSizes{nullptr};
        u64 m_rows{0u};
        u64 m_columns{0u};
        u32 m_stringRangeMin{0u};
        u32 m_stringRangeMax{0u};
    };
}