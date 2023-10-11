#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>

#include <memory_resource>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oblo::vk
{
    struct texture;
}

namespace oblo::vk
{
    enum class graph_error : u8
    {
        node_not_found,
        pin_not_found,
        missing_input,
        input_already_connected,
        not_a_dag,
    };

    class render_graph;

    class topology_builder
    {
    public:
        template <typename T>
        topology_builder& add_node();

        template <typename T>
        topology_builder& add_input(std::string_view name);

        template <typename T>
        topology_builder& add_output(std::string_view name);

        template <typename T, typename NodeFrom, typename NodeTo>
        topology_builder& connect(resource<T>(NodeFrom::*from), resource<T>(NodeTo::*to));

        template <typename T, typename NodeFrom, typename NodeTo>
        topology_builder& connect(data<T>(NodeFrom::*from), data<T>(NodeTo::*to));

        template <typename T, typename NodeTo>
        topology_builder& connect_input(std::string_view inputName, resource<T>(NodeTo::*to));

        template <typename T, typename NodeTo>
        topology_builder& connect_input(std::string_view inputName, data<T>(NodeTo::*to));

        template <typename T, typename NodeFrom>
        topology_builder& connect_output(resource<T>(NodeFrom::*from), std::string_view outputName);

        template <typename T, typename NodeFrom>
        topology_builder& connect_output(data<T>(NodeFrom::*from), std::string_view outputName);

        expected<render_graph, graph_error> build();

    private:
        struct node_desc;

        void register_pin(...);

        void register_pin(node_desc& nodeDesc, resource<texture>* pin);

        template <typename T>
        void register_pin(node_desc& nodeDesc, data<T>* pin);

    private:
        enum class pin_kind : u8;

    private:
        template <typename T, typename U>
        static u32 get_member_offset(U(T::*m));

        template <typename T>
        constexpr static pin_kind get_resource_pin_kind();

    private:
        using data_factory = any (*)();

        struct edge_desc
        {
            type_id targetNode;
            type_id dataType;
            u32 sourceOffset;
            u32 targetOffset;
            pin_kind kind;
        };

        struct node_desc
        {
            any node;
            usize nodeIndex;
            std::pmr::vector<edge_desc> outEdges;
            u32 firstTexturePin;
            u32 lastTexturePin;
            u32 firstDataPin;
            u32 lastDataPin;
            std::vector<data_factory> dataFactories;
            build_fn build;
            execute_fn execute;
        };

        struct input_desc : pin
        {
            std::pmr::vector<edge_desc> outEdges;
            data_factory factory;
        };

        struct output_desc : pin
        {
            std::pmr::vector<edge_desc> inEdges;
        };

    private:
        std::pmr::unsynchronized_pool_resource m_pool;
        std::pmr::unordered_map<type_id, node_desc> m_nodes{&m_pool};
        std::pmr::vector<input_desc> m_inputs{&m_pool};
        std::pmr::vector<output_desc> m_outputs{&m_pool};

        u32 m_virtualTextureId{0};
        u32 m_virtualDataId{0};
    };

    enum class topology_builder::pin_kind : u8
    {
        data,
        texture,
    };

    template <typename T>
    constexpr topology_builder::pin_kind topology_builder::get_resource_pin_kind()
    {
        if constexpr (std::is_same_v<T, texture>)
        {
            return pin_kind::texture;
        }
    }

    template <typename T>
    topology_builder& topology_builder::add_node()
    {
        const auto [it, ok] =
            m_nodes.emplace(get_type_id<T>(), node_desc{.outEdges = std::pmr::vector<edge_desc>{&m_pool}});

        if (ok)
        {
            node_desc& nodeDesc = it->second;
            T& instance = nodeDesc.node.template emplace<T>();

            const u32 firstTexturePin{m_virtualTextureId};
            const u32 firstDataPin{m_virtualDataId};

            struct_apply([this, &nodeDesc](auto&... fields) { (this->register_pin(nodeDesc, &fields), ...); },
                         instance);

            const u32 lastTexturePin{m_virtualTextureId};
            const u32 lastDataPin{m_virtualDataId};

            nodeDesc.firstTexturePin = firstTexturePin;
            nodeDesc.lastTexturePin = lastTexturePin;

            nodeDesc.firstDataPin = firstDataPin;
            nodeDesc.lastDataPin = lastDataPin;

            if constexpr (requires(T& node, runtime_builder& builder) { node.build(builder); })
            {
                nodeDesc.build = [](void* node, runtime_builder& builder) { static_cast<T*>(node)->build(builder); };
            }

            if constexpr (requires(T& node, runtime_context& context) { node.execute(context); })
            {
                nodeDesc.execute = [](void* node, runtime_context& context)
                { static_cast<T*>(node)->execute(context); };
            }
        }

        return *this;
    }

    template <typename T>
    topology_builder& topology_builder::add_input(std::string_view name)
    {
        auto& input = m_inputs.emplace_back();
        input.name = name;
        input.value.emplace<T>();
        input.typeId = get_type_id<T>();
        input.outEdges = std::pmr::vector<edge_desc>{&m_pool};
        input.factory = [] { return any{T{}}; };
        return *this;
    }

    template <typename T>
    topology_builder& topology_builder::add_output(std::string_view name)
    {
        auto& output = m_outputs.emplace_back();
        output.name = name;
        output.value.emplace<T>();
        output.typeId = get_type_id<T>();
        output.inEdges = std::pmr::vector<edge_desc>{&m_pool};
        return *this;
    }

    template <typename T, typename NodeFrom, typename NodeTo>
    topology_builder& topology_builder::connect(resource<T>(NodeFrom::*from), resource<T>(NodeTo::*to))
    {
        const auto it = m_nodes.find(get_type_id<NodeFrom>());

        if (it != m_nodes.end())
        {
            it->second.outEdges.push_back({
                .targetNode = get_type_id<NodeTo>(),
                .dataType = get_type_id<T>(),
                .sourceOffset = get_member_offset(from),
                .targetOffset = get_member_offset(to),
                .kind = get_resource_pin_kind<T>(),
            });
        }

        return *this;
    }

    template <typename T, typename NodeFrom, typename NodeTo>
    topology_builder& topology_builder::connect(data<T>(NodeFrom::*from), data<T>(NodeTo::*to))
    {
        const auto it = m_nodes.find(get_type_id<NodeFrom>());

        if (it != m_nodes.end())
        {
            it->second.outEdges.push_back({
                .targetNode = get_type_id<NodeTo>(),
                .dataType = get_type_id<T>(),
                .sourceOffset = get_member_offset(from),
                .targetOffset = get_member_offset(to),
                .kind = pin_kind::data,
            });
        }

        return *this;
    }

    template <typename T, typename NodeTo>
    topology_builder& topology_builder::connect_input(std::string_view inputName, resource<T>(NodeTo::*to))
    {
        for (auto& in : m_inputs)
        {
            if (in.name == inputName)
            {
                OBLO_ASSERT(get_type_id<h32<T>>() == in.typeId);

                in.outEdges.push_back({
                    .targetNode = get_type_id<NodeTo>(),
                    .dataType = get_type_id<T>(),
                    .targetOffset = get_member_offset(to),
                    .kind = get_resource_pin_kind<T>(),
                });

                break;
            }
        }

        return *this;
    }

    template <typename T, typename NodeTo>
    topology_builder& topology_builder::connect_input(std::string_view inputName, data<T>(NodeTo::*to))
    {
        for (auto& in : m_inputs)
        {
            if (in.name == inputName)
            {
                OBLO_ASSERT(get_type_id<T>() == in.typeId);

                in.outEdges.push_back({
                    .targetNode = get_type_id<NodeTo>(),
                    .dataType = get_type_id<T>(),
                    .targetOffset = get_member_offset(to),
                    .kind = pin_kind::data,
                });

                break;
            }
        }

        return *this;
    }

    template <typename T, typename NodeFrom>
    topology_builder& topology_builder::connect_output(resource<T>(NodeFrom::*from), std::string_view outputName)
    {
        for (auto& out : m_outputs)
        {
            if (out.name == outputName)
            {
                OBLO_ASSERT(get_type_id<h32<T>>() == out.typeId);

                out.inEdges.push_back({
                    .targetNode = get_type_id<NodeFrom>(),
                    .dataType = get_type_id<T>(),
                    .targetOffset = get_member_offset(from),
                    .kind = get_resource_pin_kind<T>(),
                });

                break;
            }
        }

        return *this;
    }

    template <typename T, typename NodeFrom>
    topology_builder& topology_builder::connect_output(data<T>(NodeFrom::*from), std::string_view outputName)
    {
        for (auto& out : m_outputs)
        {
            if (out.name == outputName)
            {
                OBLO_ASSERT(get_type_id<T>() == out.typeId);

                out.inEdges.push_back({
                    .targetNode = get_type_id<NodeFrom>(),
                    .dataType = get_type_id<T>(),
                    .targetOffset = get_member_offset(from),
                    .kind = pin_kind::data,
                    .factory = [] { return any{T{}}; },
                });

                break;
            }
        }

        return *this;
    }

    template <typename T, typename U>
    u32 topology_builder::get_member_offset(U(T::*m))
    {
        alignas(T) std::byte buf[sizeof(T)];
        auto& t = *start_lifetime_as<T>(buf);

        u8* const bStructPtr = reinterpret_cast<u8*>(&t);
        u8* const bMemberPtr = reinterpret_cast<u8*>(&(t.*m));
        return u32(bMemberPtr - bStructPtr);
    }

    inline void topology_builder::register_pin(...) {}

    inline void topology_builder::register_pin(node_desc&, resource<texture>* pin)
    {
        // u8* const bMemberPtr = reinterpret_cast<u8*>(&pin);

        const u32 id = ++m_virtualTextureId;
        // const u32 offset = u32(bMemberPtr - nodePtr);

        // nodeDesc->texturePins.push_back({.id = id, .offset = offset});
        pin->value = id;
    }

    template <typename T>
    void topology_builder::register_pin(node_desc& nodeDesc, data<T>* pin)
    {
        // u8* const bMemberPtr = reinterpret_cast<u8*>(&pin);

        const u32 id = ++m_virtualDataId;
        // const u32 offset = u32(bMemberPtr - nodePtr);

        // nodeDesc->dataPins.push_back({.id = id, .offset = offset});
        pin->value = id;
        nodeDesc.dataFactories.emplace_back([] { return any{T{}}; });
    }
}