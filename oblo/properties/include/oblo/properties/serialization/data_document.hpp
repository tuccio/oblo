#pragma once

#include <oblo/core/types.hpp>
#include <oblo/properties/serialization/data_node.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace oblo
{
    class property_registry;
    enum class property_kind : u8;

    struct data_node;
    class data_node_iterator;

    class data_document
    {
    public:
        data_document();
        data_document(const data_document&) = delete;
        data_document(data_document&&) noexcept = delete;

        ~data_document();

        data_document& operator=(const data_document&) = delete;
        data_document& operator=(data_document&&) noexcept = delete;

        void init(u32 firstChunkSize = 1u << 15);

        u32 get_root() const;

        void child_value(u32 parent, std::string_view key, property_kind kind, std::span<const std::byte> data);

        std::span<const data_node> get_nodes() const;

        data_node_iterator begin() const;
        data_node_iterator end() const;

    private:
        struct data_chunk;

    private:
        void* allocate(usize size, usize alignment);
        data_chunk* allocate_chunk(u8 exponent);

    private:
        std::vector<data_node> m_nodes;
        data_chunk* m_firstChunk{};
        data_chunk* m_currentChunk{};
        u32 m_firstChunkSize{};
        u8 m_chunksCount{};
    };

//     data_node_iterator data_document::begin() const
//     {
//         return data_node_iterator{m_nodes, 0};
//     }

//     data_node_iterator data_document::end() const
//     {
//         return data_node_iterator{m_nodes, m_nodes.size()};
//     }
}