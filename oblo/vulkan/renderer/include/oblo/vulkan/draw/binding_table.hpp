#pragma once

#include <oblo/core/deque.hpp>
#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/pair.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/forward.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/texture.hpp>

#include <initializer_list>
#include <span>

namespace oblo
{
    class string;
}

namespace oblo::vk
{
    enum class bindable_resource_kind : u8
    {
        none,
        buffer,
        texture,
        acceleration_structure,
    };

    struct bindable_buffer
    {
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };

    struct bindable_texture
    {
        VkImageView view;
        VkImageLayout layout;
    };

    struct bindable_acceleration_structure
    {
        VkAccelerationStructureKHR handle;
    };

    struct bindable_object
    {
        bindable_resource_kind kind;

        union {
            bindable_buffer buffer;
            bindable_texture texture;
            bindable_acceleration_structure accelerationStructure;
        };
    };

    constexpr bindable_object make_bindable_object(const vk::buffer& b)
    {
        return {.kind = bindable_resource_kind::buffer, .buffer = {b.buffer, b.offset, b.size}};
    }

    constexpr bindable_object make_bindable_object(VkImageView view, VkImageLayout layout)
    {
        return {.kind = bindable_resource_kind::texture, .texture = {view, layout}};
    }

    constexpr bindable_object make_bindable_object(VkAccelerationStructureKHR accelerationStructure)
    {
        return {.kind = bindable_resource_kind::acceleration_structure,
            .accelerationStructure = {accelerationStructure}};
    }

    using binding_table = flat_dense_map<h32<string>, bindable_object>;

    struct bindable_resource
    {
        bindable_resource_kind kind;

        union {
            resource<buffer> buffer;
            resource<texture> texture;
            resource<acceleration_structure> accelerationStructure;
        };
    };

    class binding_table2
    {
    public:
        void bind(hashed_string_view name, resource<buffer> r)
        {
            m_kv.emplace_back(name, bindable_resource{.kind = bindable_resource_kind::buffer, .buffer = r});
        }

        void bind(hashed_string_view name, resource<texture> r)
        {
            m_kv.emplace_back(name, bindable_resource{.kind = bindable_resource_kind::texture, .texture = r});
        }

        void bind(hashed_string_view name, resource<acceleration_structure> r)
        {
            m_kv.emplace_back(name,
                bindable_resource{.kind = bindable_resource_kind::acceleration_structure, .accelerationStructure = r});
        }

        void bind_buffers(std::initializer_list<pair<hashed_string_view, resource<buffer>>> list)
        {
            for (const auto& [k, v] : list)
            {
                bind(k, v);
            }
        }

        void bind_textures(std::initializer_list<pair<hashed_string_view, resource<texture>>> list)
        {
            for (const auto& [k, v] : list)
            {
                bind(k, v);
            }
        }

        const bindable_resource* try_find(hashed_string_view name) const
        {
            for (const auto& [k, v] : m_kv)
            {
                if (k == name)
                {
                    return &v;
                }
            }

            return nullptr;
        }

        void clear()
        {
            m_kv.clear();
        }

    private:
        struct key_value
        {
            hashed_string_view key;
            bindable_resource value;
        };

    private:
        deque<key_value> m_kv;
    };

    class binding_tables_span
    {
    public:
        binding_tables_span() = default;
        binding_tables_span(const binding_tables_span&) = default;
        binding_tables_span(const binding_table2& t) : m_table{&t}, m_count{1} {}

        binding_tables_span(std::span<const binding_table2* const> tables) :
            m_array{tables.data()}, m_count{tables.size()}
        {
            if (m_count == 1)
            {
                m_table = m_array[0];
            }
        }

        template <usize N>
        binding_tables_span(const binding_table2* (&array)[N]) : binding_tables_span{std::span{array}}
        {
        }

        std::span<const binding_table2* const> span() const&
        {
            auto* const array = m_count == 1 ? &m_table : m_array;
            return std::span<const binding_table2* const>{array, m_count};
        }

    private:
        const binding_table2* m_table{};
        const binding_table2* const* m_array{};
        usize m_count{};
    };
}