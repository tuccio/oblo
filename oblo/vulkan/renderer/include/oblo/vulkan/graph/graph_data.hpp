#pragma once

#include <oblo/core/type_id.hpp>

#include <string>

namespace oblo::vk
{
    class runtime_builder;
    class runtime_context;

    using construct_fn = void (*)(void*);
    using destruct_fn = void (*)(void*);
    using build_fn = void (*)(void*, runtime_builder&);
    using execute_fn = void (*)(void*, runtime_context&);

    struct node
    {
        void* node;
        type_id typeId;
        construct_fn construct;
        destruct_fn destruct;
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