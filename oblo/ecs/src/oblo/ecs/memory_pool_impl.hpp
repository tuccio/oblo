#pragma once

#include <oblo/core/types.hpp>

#include <memory_resource>

namespace oblo::ecs
{
    struct memory_pool_impl
    {
        std::pmr::unsynchronized_pool_resource poolResource;

        template <typename T>
        T* create_array_uninitialized(usize count)
        {
            void* vptr = poolResource.allocate(sizeof(T) * count, alignof(T));
            return new (vptr) T[count];
        }

        template <typename T>
        void create_array_uninitialized(T*& ptr, usize count)
        {
            ptr = create_array_uninitialized<T>(count);
        }

        template <typename T>
        void* allocate()
        {
            return poolResource.allocate(sizeof(T), alignof(T));
        }

        template <typename T>
        void* allocate(usize count)
        {
            return poolResource.allocate(sizeof(T) * count, alignof(T));
        }

        template <typename T>
        void deallocate(T* ptr)
        {
            poolResource.deallocate(ptr, sizeof(T), alignof(T));
        }

        template <typename T>
        void deallocate_array(T* ptr, usize count)
        {
            poolResource.deallocate(ptr, sizeof(T) * count, alignof(T));
        }

        template <typename T>
        T* create_uninitialized()
        {
            return new (poolResource.allocate(sizeof(T), alignof(T))) T;
        }
    };

}