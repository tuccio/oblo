#pragma once

#include <oblo/core/types.hpp>

namespace oblo
{
    enum class property_kind : u8;

    enum class data_node_kind : u8
    {
        object,
        value,
    };

    struct data_node
    {
        static constexpr u32 Invalid{~0u};

        data_node_kind kind;
        property_kind valueKind;
        u16 keyLen;
        u32 nextSibling;
        const char* key;

        union {
            struct
            {
                u32 firstChild;
                u32 lastChild;
            } object;

            struct
            {
                void* data;
            } value;
        };
    };

    // class data_node_iterator
    // {
    // public:
    //     using iterator_category = std::forward_iterator_tag;
    //     using difference_type = std::ptrdiff_t;
    //     using value_type = data_node;
    //     using pointer = const data_node*;
    //     using reference = const data_node&;

    // public:
    //     data_node_iterator(std::span<const data_node> nodes, u32 index) : m_nodes{nodes}, m_index{index} {}
    //     data_node_iterator(const member_iterator&) = default;
    //     data_node_iterator& operator=(const member_iterator&) = default;

    //     reference operator*() const
    //     {
    //         return m_nodes[m_index];
    //     }
    //     pointer operator->()
    //     {
    //         return m_nodes.data() + m_index;
    //     }

    //     data_node_iterator& operator++()
    //     {
    //         m_index++;
    //         return *this;
    //     }

    //     data_node_iterator operator++(int)
    //     {
    //         member_iterator tmp = *this;
    //         ++(*this);
    //         return tmp;
    //     }

    //     friend bool operator==(const data_node_iterator& a, const data_node_iterator& b)
    //     {
    //         return a.m_index == b.m_index;
    //     };

    // private:
    //     std::span<const data_node> m_nodes;
    //     usize m_index{};
    // };
}