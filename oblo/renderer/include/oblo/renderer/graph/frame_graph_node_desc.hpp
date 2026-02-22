#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/core/struct_apply.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/renderer/graph/pins.hpp>

// TODO: Just temporarily, we need to type erase frame_graph_buffer_impl and frame_graph_texture_impl
#include <oblo/renderer/platform/renderer_platform.hpp>

namespace oblo
{
    class frame_graph_init_context;
    class frame_graph_build_context;
    class frame_graph_execute_context;

    using frame_graph_construct_fn = void (*)(void*);
    using frame_graph_destruct_fn = void (*)(void*);
    using frame_graph_clear_fn = void (*)(void*);
    using frame_graph_init_fn = void (*)(void*, const frame_graph_init_context&);
    using frame_graph_build_fn = void (*)(void*, const frame_graph_build_context&);
    using frame_graph_execute_fn = void (*)(void*, const frame_graph_execute_context&);

    struct frame_graph_data_desc
    {
        u32 size;
        u32 alignment;
        frame_graph_construct_fn construct;
        frame_graph_destruct_fn destruct;
        type_id typeId;

        template <typename T>
        static frame_graph_data_desc make()
        {
            return {
                .size = sizeof(T),
                .alignment = alignof(T),
                .construct = [](void* p) { new (p) T{}; },
                .destruct = [](void* p) { static_cast<T*>(p)->~T(); },
                .typeId = get_type_id<T>(),
            };
        }
    };

    struct frame_graph_node_pin
    {
        u32 offset;
        frame_graph_data_desc typeDesc;
        frame_graph_clear_fn clearSink;
    };

    struct frame_graph_node_desc
    {
        string name;

        frame_graph_data_desc typeDesc;

        frame_graph_init_fn init;
        frame_graph_build_fn build;
        frame_graph_execute_fn execute;

        dynamic_array<frame_graph_node_pin> pins;
    };

    namespace detail
    {
        inline void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const pin::texture* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<frame_graph_texture_impl>(),
            });
        }

        inline void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const pin::buffer* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<frame_graph_buffer_impl>(),
            });
        }

        template <typename T>
        void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const pin::data<T>* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<T>(),
            });
        }

        template <typename T>
        void register_pin(frame_graph_node_desc* nodeDesc, const u8* nodePtr, const pin::data_sink<T>* pin)
        {
            const u8* const bMemberPtr = reinterpret_cast<const u8*>(pin);

            const u32 offset = u32(bMemberPtr - nodePtr);

            nodeDesc->pins.push_back({
                .offset = offset,
                .typeDesc = frame_graph_data_desc::make<pin::data_sink_container<T>>(),
                .clearSink = [](void* ptr) { static_cast<pin::data_sink_container<T>*>(ptr)->clear(); },
            });
        }

        inline void register_pin(frame_graph_node_desc*, const u8*, ...)
        {
            // Fallback for regular fields
        }
    }

    template <typename T>
    frame_graph_node_desc make_frame_graph_node_desc()
    {
        frame_graph_node_desc nodeDesc{
            .typeDesc = frame_graph_data_desc::make<T>(),
        };

        if constexpr (requires(T& node, const frame_graph_init_context& context) { node.init(context); })
        {
            nodeDesc.init = [](void* node, const frame_graph_init_context& context)
            { static_cast<T*>(node)->init(context); };
        }

        if constexpr (requires(T& node, const frame_graph_build_context& context) { node.build(context); })
        {
            nodeDesc.build = [](void* node, const frame_graph_build_context& context)
            { static_cast<T*>(node)->build(context); };
        }

        if constexpr (requires(T& node, const frame_graph_execute_context& context) { node.execute(context); })
        {
            nodeDesc.execute = [](void* node, const frame_graph_execute_context& context)
            { static_cast<T*>(node)->execute(context); };
        }

        // DIY reflection, we don't need a proper instance, just something to iterate member pointers and calculate
        // their offsets
        alignas(T) const u8 buffer[sizeof(T)]{};
        const T* instance = reinterpret_cast<const T*>(buffer);

        struct_apply(
            [instance, &nodeDesc](auto&... fields)
            {
                nodeDesc.pins.reserve(sizeof...(fields));
                (detail::register_pin(&nodeDesc, reinterpret_cast<const u8*>(instance), &fields), ...);
            },
            *instance);

        return nodeDesc;
    }
}