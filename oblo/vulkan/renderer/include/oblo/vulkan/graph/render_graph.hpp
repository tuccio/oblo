#pragma once

#include <oblo/core/debug.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace oblo::vk
{
    class renderer;
    class resource_manager;
    class resource_pool;
    class runtime_context;
    class stateful_command_buffer;
    class topology_builder;

    struct cpu_data;
    struct gpu_resource;
    struct node;
    struct pin;
    struct texture;

    class render_graph
    {
        friend class runtime_builder;
        friend class runtime_context;
        friend class topology_builder;

    public:
        render_graph();
        render_graph(const render_graph&) = delete;
        render_graph(render_graph&&) noexcept;
        render_graph& operator=(const render_graph&) = delete;
        render_graph& operator=(render_graph&&) noexcept;
        ~render_graph();

        void* find_node(type_id type);

        template <typename T>
        T* find_node()
        {
            return static_cast<T*>(find_node(get_type_id<T>()));
        }

        void* find_input(std::string_view name);

        template <typename T>
        T* find_input(std::string_view name)
        {
            return static_cast<T*>(find_input(name));
        }

        template <typename T>
        void set_input(std::string_view name, const T& value)
        {
            T* const ptr = find_input<T>(name);
            *ptr = value;
        }

        void* find_output(std::string_view name);

        template <typename T>
        T* find_output(std::string_view name)
        {
            return static_cast<T*>(find_output(name));
        }

        // This is mostly here for test purposes, since users cannot do much with it.
        // It returns the index into the array of backing texture resources which virtual
        // resources will point to.
        u32 get_backing_texture_id(resource<texture> virtualTextureId) const;

        void init(renderer& renderer);

        void build(resource_pool& resourcePool);

        void execute(renderer& renderer, resource_pool& resourcePool);

        bool copy_output(std::string_view name, h32<texture> target);

    private:
        void* access_resource_storage(u32 h) const;

        void add_transient_resource(resource<texture> texture, u32 poolIndex);
        void add_resource_transition(resource<texture> texture, VkImageLayout target);
        u32 find_pool_index(resource<texture> texture) const;

        void flush_copies(stateful_command_buffer& commandBuffer, resource_manager& resourceManager);

    private:
        struct pin_data;
        struct named_pin_data;
        struct data_storage;
        struct node_transitions;
        struct texture_transition;
        struct transient_texture;
        struct pending_copy;

        using destruct_fn = void (*)(void*);

    private:
        std::unique_ptr<std::byte[]> m_allocator;
        std::vector<node> m_nodes;
        std::vector<node_transitions> m_nodeTransitions;
        std::vector<texture_transition> m_textureTransitions;
        std::vector<transient_texture> m_transientTextures;

        std::vector<named_pin_data> m_inputs;
        std::vector<named_pin_data> m_outputs;
        std::vector<pin_data> m_pins;
        std::vector<data_storage> m_pinStorage;

        // Used to store the poolIndex while building
        std::vector<u32> m_resourcePoolId;

        std::vector<pending_copy> m_pendingCopies;
    };

    struct render_graph::named_pin_data
    {
        std::string name;
        u32 storageIndex;
    };

    struct render_graph::pin_data
    {
        u32 storageIndex;
    };

    struct render_graph::data_storage
    {
        void* ptr;
        destruct_fn destruct;
    };

    struct render_graph::node_transitions
    {
        u32 firstTextureTransition;
        u32 lastTextureTransition;
    };

    struct render_graph::texture_transition
    {
        resource<texture> texture;
        VkImageLayout target;
    };

    struct render_graph::transient_texture
    {
        resource<texture> texture;
        u32 poolIndex;
    };
}