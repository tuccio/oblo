#pragma once

#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>
#include <oblo/vulkan/graph/frame_graph_node_desc.hpp>

#include <unordered_map>

namespace oblo::vk
{
    struct frame_graph_node_desc;

    class frame_graph_registry
    {
    public:
        bool register_node(const uuid& id, frame_graph_node_desc&& desc);
        void unregister_node(const uuid& id);

        template <typename T>
        bool register_node();

        template <typename T>
        uuid get_uuid() const;

        const frame_graph_node_desc* find_node(const uuid& id) const;

    private:
        std::unordered_map<uuid, frame_graph_node_desc> m_nodes;
    };

    template <typename T>
    inline bool frame_graph_registry::register_node()
    {
        const uuid id = get_uuid<T>();
        return register_node(id, make_frame_graph_node_desc<T>());
    }

    template <typename T>
    inline uuid frame_graph_registry::get_uuid() const
    {
        constexpr type_id t = get_type_id<T>();
        return uuid_namespace_generator{}.generate(t.name);
    }
}