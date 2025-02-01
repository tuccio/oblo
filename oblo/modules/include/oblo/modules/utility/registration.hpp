#pragma once

#include <oblo/core/allocator.hpp>
#include <oblo/core/platform/compiler.hpp>
#include <oblo/modules/module_interface.hpp>

#include <new>

#define OBLO_MODULE_INSTANTIATE_SYM _oblo_intantiate_module
#define OBLO_MODULE_REGISTER(Class)                                                                                    \
    extern "C" OBLO_SHARED_LIBRARY_EXPORT oblo::module_interface* OBLO_MODULE_INSTANTIATE_SYM(                         \
        oblo::allocator* allocator)                                                                                    \
    {                                                                                                                  \
        using T = Class;                                                                                               \
        void* const ptr = allocator->allocate(sizeof(T), alignof(T));                                                  \
        return new (ptr) T{};                                                                                          \
    }