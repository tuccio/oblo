#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/lifetime.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/graph_data.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/graph/render_graph.hpp>

#include <memory_resource>
#include <string_view>
#include <unordered_map>
#include <vector>

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
    struct texture;

    template <typename T>
    concept render_node = requires(T& node, const runtime_builder& builder, const runtime_context& context) {
        {
            T{}
        };
        {
            node.build(builder)
        };
    };

    class topology_builder
    {
    public:
        template <typename T>
            requires render_node<T>
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
        template <typename BackingType, typename T, typename NodeFrom, typename NodeTo>
        topology_builder& connect_impl(T(NodeFrom::*from), T(NodeTo::*to));

    private:
        struct node_desc;

        void register_pin(node_desc*, const u8*, ...) {}

        void register_pin(node_desc* nodeDesc, const u8* nodePtr, const resource<texture>* pin);
        void register_pin(node_desc* nodeDesc, const u8* nodePtr, const resource<buffer>* pin);

        template <typename T>
        void register_pin(node_desc* nodeDesc, const u8* nodePtr, const data<T>* pin);

    private:
        enum class pin_kind : u8;

    private:
        template <typename T, typename U>
        static u32 get_member_offset(U(T::*m));

        template <typename T>
        constexpr static pin_kind get_resource_pin_kind();

        template <typename T>
        consteval static pin_kind get_pin_kind(data<T>);

        consteval static pin_kind get_pin_kind(resource<texture>);

    private:
        struct type_desc
        {
            u32 size;
            u32 alignment;
            construct_fn construct;
            destruct_fn destruct;

            template <typename T>
            static type_desc make()
            {
                return {
                    .size = sizeof(T),
                    .alignment = alignof(T),
                    .construct = [](void* p) { new (p) T{}; },
                    .destruct = [](void* p) { static_cast<T*>(p)->~T(); },
                };
            }
        };

        struct edge_desc
        {
            type_id targetNode;
            type_id dataType;
            u32 sourceOffset;
            u32 targetOffset;
            pin_kind kind;
        };

        struct pin_data
        {
            u32 id;
            u32 offset;
            type_desc typeDesc;
        };

        struct node_desc
        {
            void* node;
            type_desc typeDesc;
            usize nodeIndex;
            std::pmr::vector<edge_desc> outEdges;
            u32 firstPin;
            u32 lastPin;
            std::pmr::vector<pin_data> pins;
            init_fn init;
            build_fn build;
            execute_fn execute;
        };

        struct input_desc : pin
        {
            std::pmr::vector<edge_desc> outEdges;
            type_desc typeDesc;
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

        u32 m_nextDataId{0};

        usize m_allocationSize{0};
    };

    template <typename T>
        requires render_node<T>
    topology_builder& topology_builder::add_node()
    {
        const auto [it, ok] =
            m_nodes.emplace(get_type_id<T>(), node_desc{.outEdges = std::pmr::vector<edge_desc>{&m_pool}});

        if (ok)
        {
            m_allocationSize += sizeof(T) + alignof(T) - 1;

            const T instance{};

            const u32 firstPin{m_nextDataId};

            node_desc& nodeDesc = it->second;

            struct_apply(
                [this, &instance, &nodeDesc](auto&... fields)
                {
                    nodeDesc.pins = std::pmr::vector<pin_data>{&m_pool};
                    nodeDesc.pins.reserve(sizeof...(fields));
                    (this->register_pin(&nodeDesc, reinterpret_cast<const u8*>(&instance), &fields), ...);
                },
                instance);

            const u32 lastPin{m_nextDataId};

            nodeDesc.typeDesc = type_desc::make<T>();

            nodeDesc.firstPin = firstPin;
            nodeDesc.lastPin = lastPin;

            if constexpr (requires(T& node, const init_context& context) { node.init(context); })
            {
                nodeDesc.init = [](void* node, const init_context& context) { static_cast<T*>(node)->init(context); };
            }

            if constexpr (requires(T& node, const runtime_builder& builder) { node.build(builder); })
            {
                nodeDesc.build = [](void* node, const runtime_builder& builder)
                { static_cast<T*>(node)->build(builder); };
            }

            if constexpr (requires(T& node, const runtime_context& context) { node.execute(context); })
            {
                nodeDesc.execute = [](void* node, const runtime_context& context)
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
        input.typeId = get_type_id<T>();
        input.outEdges = std::pmr::vector<edge_desc>{&m_pool};
        input.typeDesc = type_desc::make<T>();

        m_allocationSize += sizeof(T) + alignof(T) - 1;

        return *this;
    }

    template <typename T>
    topology_builder& topology_builder::add_output(std::string_view name)
    {
        auto& output = m_outputs.emplace_back();
        output.name = name;
        output.typeId = get_type_id<T>();
        output.inEdges = std::pmr::vector<edge_desc>{&m_pool};
        return *this;
    }

    template <typename BackingType, typename T, typename NodeFrom, typename NodeTo>
    topology_builder& topology_builder::connect_impl(T(NodeFrom::*from), T(NodeTo::*to))
    {
        const auto it = m_nodes.find(get_type_id<NodeFrom>());

        if (it != m_nodes.end())
        {
            it->second.outEdges.push_back({
                .targetNode = get_type_id<NodeTo>(),
                .dataType = get_type_id<BackingType>(),
                .sourceOffset = get_member_offset(from),
                .targetOffset = get_member_offset(to),
            });
        }

        return *this;
    }

    template <typename T, typename NodeFrom, typename NodeTo>
    topology_builder& topology_builder::connect(resource<T>(NodeFrom::*from), resource<T>(NodeTo::*to))
    {
        return connect_impl<h32<T>>(from, to);
    }

    template <typename T, typename NodeFrom, typename NodeTo>
    topology_builder& topology_builder::connect(data<T>(NodeFrom::*from), data<T>(NodeTo::*to))
    {
        return connect_impl<T>(from, to);
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
                    .typeDesc = type_desc::make<T>(),
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

    inline void topology_builder::register_pin(node_desc* nodeDesc, const u8* nodePtr, const resource<texture>* pin)
    {
        const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

        const u32 id = m_nextDataId++;
        const u32 offset = u32(bMemberPtr - nodePtr);

        m_allocationSize += sizeof(h32<texture>) + alignof(h32<texture>) - 1;

        nodeDesc->pins.push_back({.id = id, .offset = offset, .typeDesc = type_desc::make<h32<texture>>()});
    }

    inline void topology_builder::register_pin(node_desc* nodeDesc, const u8* nodePtr, const resource<buffer>* pin)
    {
        const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

        const u32 id = m_nextDataId++;
        const u32 offset = u32(bMemberPtr - nodePtr);

        m_allocationSize += sizeof(buffer) + alignof(buffer) - 1;

        nodeDesc->pins.push_back({.id = id, .offset = offset, .typeDesc = type_desc::make<buffer>()});
    }

    template <typename T>
    void topology_builder::register_pin(node_desc* nodeDesc, const u8* nodePtr, const data<T>* pin)
    {
        const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

        const u32 id = m_nextDataId++;
        const u32 offset = u32(bMemberPtr - nodePtr);

        m_allocationSize += sizeof(T) + alignof(T) - 1;

        nodeDesc->pins.push_back({.id = id, .offset = offset, .typeDesc = type_desc::make<T>()});
    }
}