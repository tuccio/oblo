#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/vulkan/buffer.hpp>
#include <oblo/vulkan/graph/pins.hpp>
#include <oblo/vulkan/texture.hpp>

#include <string>

namespace oblo::vk
{
    class init_context;
    class runtime_builder;
    class runtime_context;

    using frame_graph_construct_fn = void (*)(void*);
    using frame_graph_destruct_fn = void (*)(void*);
    using frame_graph_init_fn = void (*)(void*, const init_context&);
    using frame_graph_build_fn = void (*)(void*, const runtime_builder&);
    using frame_graph_execute_fn = void (*)(void*, const runtime_context&);

    struct frame_graph_data_desc
    {
        u32 size;
        u32 alignment;
        frame_graph_construct_fn construct;
        frame_graph_destruct_fn destruct;

        template <typename T>
        static frame_graph_data_desc make()
        {
            return {
                .size = sizeof(T),
                .alignment = alignof(T),
                .construct = [](void* p) { new (p) T{}; },
                .destruct = [](void* p) { static_cast<T*>(p)->~T(); },
            };
        }
    };

    struct frame_graph_node_pin
    {
        u32 offset;
        frame_graph_data_desc typeDesc;
    };

    struct frame_graph_node_desc
    {
        std::string name;

        frame_graph_construct_fn construct;
        frame_graph_destruct_fn destruct;
        frame_graph_init_fn init;
        frame_graph_build_fn build;
        frame_graph_execute_fn execute;

        dynamic_array<frame_graph_node_pin> pins;
    };

    namespace detail
    {
        inline void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const resource<texture>* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<h32<texture>>(),
            });
        }

        inline void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const resource<buffer>* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<buffer>(),
            });
        }

        template <typename T>
        void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const data<T>* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<T>(),
            });
        }
    }

    template <typename T>
    frame_graph_node_desc make_frame_graph_node_desc()
    {
        frame_graph_node_desc nodeDesc;

        const T instance{};

        nodeDesc.construct = [](void* ptr) { new (ptr) T{}; };
        nodeDesc.destruct = [](void* ptr) { static_cast<T*>(ptr)->~T(); };

        if constexpr (requires(T& node, const init_context& context) { node.init(context); })
        {
            nodeDesc.init = [](void* node, const init_context& context) { static_cast<T*>(node)->init(context); };
        }

        if constexpr (requires(T& node, const runtime_builder& builder) { node.build(builder); })
        {
            nodeDesc.build = [](void* node, const runtime_builder& builder) { static_cast<T*>(node)->build(builder); };
        }

        if constexpr (requires(T& node, const runtime_context& context) { node.execute(context); })
        {
            nodeDesc.execute = [](void* node, const runtime_context& context)
            { static_cast<T*>(node)->execute(context); };
        }

        struct_apply(
            [&instance, &nodeDesc](auto&... fields)
            {
                nodeDesc.pins.reserve(sizeof...(fields));
                (detail::register_pin(&nodeDesc, reinterpret_cast<const u8*>(&instance), &fields), ...);
            },
            instance);

        return nodeDesc;
    }
}