#pragma once

#include <oblo/core/type_id.hpp>

#include <string>

namespace oblo::vk
{
    class init_context;
    class runtime_builder;
    class runtime_context;

    using construct_fn = void (*)(void*);
    using destruct_fn = void (*)(void*);
    using init_fn = void (*)(void*, const init_context&);
    using build_fn = void (*)(void*, const runtime_builder&);
    using execute_fn = void (*)(void*, const runtime_context&);

    struct node
    {
        void* node;
        type_id typeId;
        construct_fn construct;
        destruct_fn destruct;
        init_fn init;
        build_fn build;
        execute_fn execute;
    };

    struct pin
    {
        type_id typeId;
        std::string name;
    };

    struct gpu_resource
    {
        u32 id;
        type_id type;
    };

    struct cpu_data
    {
        void* ptr;
        type_id type;
    };
}