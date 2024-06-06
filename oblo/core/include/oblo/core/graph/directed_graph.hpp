#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/handle_flat_pool_map.hpp>

#include <utility>

namespace oblo
{
    struct empty
    {
    };

    template <typename Vertex = empty, typename Edge = empty>
    struct directed_graph
    {
    public:
        struct vertex_tag;
        struct edge_tag;

        using vertex_handle = h32<vertex_tag>;
        using edge_handle = h32<edge_tag>;

        struct edge_reference
        {
            vertex_handle vertex;
            edge_handle handle;
        };

    public:
        template <typename... Args>
        vertex_handle add_vertex(Args&&... args)
        {
            const auto [it, key] = m_vertices.emplace(std::forward<Args>(args)...);
            OBLO_ASSERT(key);

            return vertex_handle{key};
        }

        template <typename... Args>
        edge_handle add_edge(vertex_handle from, vertex_handle to, Args&&... args)
        {
            vertex_storage* const src = m_vertices.try_find(from);
            vertex_storage* const dst = m_vertices.try_find(to);

            OBLO_ASSERT(src && dst);

            if (!src || !dst)
            {
                return {};
            }

            const auto [it, key] = m_edges.emplace(std::forward<Args>(args)...);
            const edge_handle edge{key};

            src->outEdges.emplace_back(to, edge);
            dst->inEdges.emplace_back(to, edge);

            return edge;
        }

        void remove_edge(edge_handle edge)
        {
            auto* const storage = m_edges.try_find(edge);
            remove_edge(storage->from, storage->to);
        }

        void remove_edge(vertex_handle from, vertex_handle to)
        {
            vertex_storage* const src = m_vertices.try_find(from);
            vertex_storage* const dst = m_vertices.try_find(to);

            OBLO_ASSERT(src && dst);

            edge_handle edge{};

            for (auto it = src->outEdges.begin(); it != src->outEdges.end(); ++it)
            {
                if (*it == to)
                {
                    edge = it->handle;
                    src->outEdges.erase_unordered(it);
                    break;
                }
            }

            OBLO_ASSERT(edge);

            for (auto it = dst->inEdges.begin(); it != dst->inEdges.end(); ++it)
            {
                if (*it == to)
                {
                    dst->inEdges.erase_unordered(it);
                    break;
                }
            }

            m_edges.erase(edge);
        }

        Vertex& get_vertex(vertex_handle vertex)
        {
            auto* const storage = m_vertices.try_find(vertex);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        const Vertex& get_vertex(vertex_handle vertex) const
        {
            auto* const storage = m_vertices.try_find(vertex);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        Edge& get_edge(edge_handle edge)
        {
            auto* const storage = m_edges.try_find(edge);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        const Edge& get_edge(edge_handle edge) const
        {
            auto* const storage = m_edges.try_find(edge);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        Vertex& operator[](vertex_handle vertex)
        {
            return get_vertex(vertex);
        }

        const Vertex& operator[](vertex_handle vertex) const
        {
            return get_vertex(vertex);
        }

        Edge& operator[](edge_handle edge)
        {
            return get_edge(edge);
        }

        const Edge& operator[](edge_handle edge) const
        {
            return get_edge(edge);
        }

        std::span<const vertex_handle> get_vertices() const
        {
            return m_vertices.keys();
        }

        std::span<const edge_handle> get_edges() const
        {
            return m_edges.keys();
        }

        std::span<const edge_reference> get_in_edges(vertex_handle vertex) const
        {
            auto* const storage = m_vertices.try_find(vertex);
            OBLO_ASSERT(storage);
            return storage->inEdges;
        }

        std::span<const edge_reference> get_out_edges(vertex_handle vertex) const
        {
            auto* const storage = m_vertices.try_find(vertex);
            OBLO_ASSERT(storage);
            return storage->outEdges;
        }

        usize get_vertex_count() const
        {
            return m_vertices.size();
        }

        usize get_edge_count() const
        {
            return m_edges.size();
        }

        usize get_dense_index(vertex_handle vertex) const
        {
            auto* const ptr = m_vertices.try_find(vertex);
            OBLO_ASSERT(ptr);
            return usize(ptr - m_vertices.values().data());
        }

    private:
        struct vertex_storage
        {
            template <typename... Args>
            vertex_storage(Args&&... args) : data{std::forward<Args>(args)...}
            {
            }

            Vertex data;
            dynamic_array<edge_reference> inEdges;
            dynamic_array<edge_reference> outEdges;
        };

        struct edge_storage
        {
            template <typename... Args>
            edge_storage(Args&&... args) : data{std::forward<Args>(args)...}
            {
            }

            Edge data;
            vertex_handle from;
            vertex_handle to;
        };

    private:
        h32_flat_pool_dense_map<vertex_tag, vertex_storage> m_vertices;
        h32_flat_pool_dense_map<edge_tag, edge_storage> m_edges;
    };
}