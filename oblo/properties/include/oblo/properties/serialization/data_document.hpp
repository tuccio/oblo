#pragma once

#include <oblo/core/expected.hpp>
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

    class data_document
    {
    public:
        enum class error : u8;

    public:
        data_document();
        data_document(const data_document&) = delete;
        data_document(data_document&&) noexcept = delete;

        ~data_document();

        data_document& operator=(const data_document&) = delete;
        data_document& operator=(data_document&&) noexcept = delete;

        void init(u32 firstChunkSize = 1u << 15);

        u32 get_root() const;

        u32 child_object(u32 parent, std::string_view key);
        void child_value(u32 parent, std::string_view key, property_kind kind, std::span<const std::byte> data);

        std::span<const data_node> get_nodes() const;

        u32 find_child(u32 parent, std::string_view name) const;
        std::string_view get_node_name(u32 node) const;

        expected<f32, error> read_f32(u32 node) const;

    private:
        struct data_chunk;

    private:
        void* allocate(usize size, usize alignment);
        data_chunk* allocate_chunk(u8 exponent);

        void append_new_child(data_node& parent, u32 newChild);

    private:
        std::vector<data_node> m_nodes;
        data_chunk* m_firstChunk{};
        data_chunk* m_currentChunk{};
        u32 m_firstChunkSize{};
        u8 m_chunksCount{};
    };

    enum class data_document::error : u8
    {
        node_kind_mismatch,
        value_kind_mismatch,
    };

    inline u32 data_document::find_child(u32 parent, std::string_view name) const
    {
        if (m_nodes[parent].kind != data_node_kind::object)
        {
            return data_node::Invalid;
        }

        for (u32 index = m_nodes[parent].object.firstChild; index != data_node::Invalid;
             index = m_nodes[index].nextSibling)
        {
            if (get_node_name(index) == name)
            {
                return index;
            }
        }

        return data_node::Invalid;
    }

    inline std::string_view data_document::get_node_name(u32 node) const
    {
        auto& n = m_nodes[node];
        return std::string_view{n.key, n.keyLen};
    }
}