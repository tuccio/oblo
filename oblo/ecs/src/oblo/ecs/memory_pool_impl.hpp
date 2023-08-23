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

    template <typename T>
    struct pooled_array
    {
        static constexpr u32 MinAllocation{16};
        static constexpr f32 GrowthFactor{1.6f};

        u32 size;
        u32 capacity;
        T* data;

        void resize_and_grow(memory_pool_impl& pool, u32 newSize)
        {
            OBLO_ASSERT(newSize >= size);

            if (newSize <= capacity)
            {
                return;
            }

            const u32 newCapacity = max(MinAllocation, u32(capacity * GrowthFactor));

            T* const newArray = pool.create_array_uninitialized<T>(newCapacity);
            std::copy_n(data, size, newArray);

            pool.deallocate_array(data);

            data = newArray;
            capacity = newCapacity;
            size = newSize;
        }

        void free(memory_pool_impl& pool)
        {
            pool.deallocate_array(data);
            *this = {};
        }
    };
}