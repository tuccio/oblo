#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/types.hpp>
#include <oblo/properties/serialization/data_node.hpp>

#include <span>
#include <string_view>

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
        data_document(data_document&&) noexcept;

        ~data_document();

        data_document& operator=(const data_document&) = delete;
        data_document& operator=(data_document&&) noexcept;

        void init(u32 firstChunkSize = 1u << 15);

        bool is_initialized() const;

        bool is_object(u32 nodeIndex) const;
        bool is_array(u32 nodeIndex) const;
        bool is_value(u32 nodeIndex) const;

        u32 get_root() const;

        u32 child_object(u32 parent, std::string_view key);
        void child_value(u32 parent, std::string_view key, property_kind kind, std::span<const byte> data);

        u32 child_array(u32 parent, std::string_view key, u32 size);
        u32 array_element(u32 array, u32 index) const;
        u32 array_size(u32 array) const;

        void make_value(u32 node, property_kind kind, std::span<const byte> data);

        std::span<const data_node> get_nodes() const;

        u32 find_child(u32 parent, std::string_view name) const;
        std::string_view get_node_name(u32 node) const;

        expected<bool, error> read_bool(u32 node) const;

        expected<f32, error> read_f32(u32 node) const;
        expected<u32, error> read_u32(u32 node) const;

    private:
        struct data_chunk;

    private:
        void* allocate(usize size, usize alignment);
        data_chunk* allocate_chunk(u8 exponent);
        const char* allocate_key(std::string_view key);

        void append_new_child(data_node& parent, u32 newChild);

    private:
        dynamic_array<data_node> m_nodes;
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

    // Used for string values in data_node
    struct data_string
    {
        const char* data;
        usize length;

        std::string_view str() const
        {
            return {data, length};
        }
    };

    template <typename T>
        requires std::is_fundamental_v<T>
    constexpr std::span<const byte> as_bytes(const T& value)
    {
        return as_bytes(std::span{&value, 1});
    }
}