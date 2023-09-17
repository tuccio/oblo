#pragma once

#include <oblo/core/types.hpp>

#include <memory>
#include <memory_resource>

namespace oblo
{
    class memory_pool : std::pmr::unsynchronized_pool_resource
    {
        std::pmr::unsynchronized_pool_resource& resource()
        {
            return *static_cast<std::pmr::unsynchronized_pool_resource*>(this);
        }

    public:
        template <typename T>
        T* create_array_uninitialized(usize count)
        {
            void* vptr = resource().allocate(sizeof(T) * count, alignof(T));
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
            return resource().allocate(sizeof(T), alignof(T));
        }

        template <typename T>
        void* allocate(usize count)
        {
            return resource().allocate(sizeof(T) * count, alignof(T));
        }

        template <typename T>
        void deallocate(T* ptr)
        {
            resource().deallocate(ptr, sizeof(T), alignof(T));
        }

        template <typename T>
        void destroy(T* ptr)
        {
            ptr->~T();
            resource().deallocate(ptr, sizeof(T), alignof(T));
        }

        template <typename T>
        void deallocate_array(T* ptr, usize count)
        {
            resource().deallocate(ptr, sizeof(T) * count, alignof(T));
        }

        template <typename T>
        T* create_uninitialized()
        {
            return new (resource().allocate(sizeof(T), alignof(T))) T;
        }
    };
}