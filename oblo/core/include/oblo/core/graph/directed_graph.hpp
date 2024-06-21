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

            /// @brief Represents the destination for out edges, or the source for in edges.
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

            OBLO_ASSERT(!has_edge(from, to), "The edge is already present");

            it->from = from;
            it->to = to;

            src->outEdges.emplace_back(to, edge);
            dst->inEdges.emplace_back(from, edge);

            return edge;
        }

        bool remove_vertex(vertex_handle vertex)
        {
            return m_vertices.erase(vertex) == 1;
        }

        bool remove_edge(edge_handle edge)
        {
            auto* const storage = m_edges.try_find(edge);

            if (!storage)
            {
                return false;
            }

            return remove_edge(storage->from, storage->to);
        }

        bool remove_edge(vertex_handle from, vertex_handle to)
        {
            edge_handle edge{};

            vertex_storage* const src = m_vertices.try_find(from);

            if (src)
            {
                for (auto it = src->outEdges.begin(); it != src->outEdges.end(); ++it)
                {
                    if (it->vertex == to)
                    {
                        edge = it->handle;
                        src->outEdges.erase_unordered(it);
                        break;
                    }
                }
            }

            vertex_storage* const dst = m_vertices.try_find(to);

            if (dst)
            {
                for (auto it = dst->inEdges.begin(); it != dst->inEdges.end(); ++it)
                {
                    if (it->vertex == to)
                    {
                        edge = it->handle;
                        dst->inEdges.erase_unordered(it);
                        break;
                    }
                }
            }

            if (edge)
            {
                m_edges.erase(edge);
                return true;
            }

            return false;
        }

        bool has_edge(vertex_handle from, vertex_handle to)
        {
            vertex_storage* const src = m_vertices.try_find(from);

            if (src)
            {
                // Assuming that if it's in this list, it's the caller's responsibility to keep it in sync
                for (auto it = src->outEdges.begin(); it != src->outEdges.end(); ++it)
                {
                    if (it->vertex == to)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        Vertex* try_get(vertex_handle vertex)
        {
            auto* const storage = m_vertices.try_find(vertex);
            return storage ? &storage->data : nullptr;
        }

        const Vertex& try_get(vertex_handle vertex) const
        {
            auto* const storage = m_vertices.try_find(vertex);
            return storage ? &storage->data : nullptr;
        }

        Vertex& get(vertex_handle vertex)
        {
            auto* const storage = m_vertices.try_find(vertex);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        const Vertex& get(vertex_handle vertex) const
        {
            auto* const storage = m_vertices.try_find(vertex);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        Edge* try_get(edge_handle edge)
        {
            auto* const storage = m_edges.try_find(edge);
            return storage ? &storage->data : nullptr;
        }

        const Edge& try_get(edge_handle edge) const
        {
            auto* const storage = m_edges.try_find(edge);
            return storage ? &storage->data : nullptr;
        }

        Edge& get(edge_handle edge)
        {
            auto* const storage = m_edges.try_find(edge);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        const Edge& get(edge_handle edge) const
        {
            auto* const storage = m_edges.try_find(edge);
            OBLO_ASSERT(storage);
            return storage->data;
        }

        Vertex& operator[](vertex_handle vertex)
        {
            return get(vertex);
        }

        const Vertex& operator[](vertex_handle vertex) const
        {
            return get(vertex);
        }

        Edge& operator[](edge_handle edge)
        {
            return get(edge);
        }

        const Edge& operator[](edge_handle edge) const
        {
            return get(edge);
        }

        vertex_handle get_source(edge_handle edge) const
        {
            return m_edges.try_find(edge)->from;
        }

        vertex_handle get_destination(edge_handle edge) const
        {
            return m_edges.try_find(edge)->to;
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