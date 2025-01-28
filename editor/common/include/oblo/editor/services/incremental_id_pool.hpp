#pragma once

#include <oblo/core/handle_pool.hpp>
#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/type_id.hpp>

#include <unordered_map>

namespace oblo::editor
{
    class incremental_id_pool
    {
    public:
        template <typename T>
        u32 acquire()
        {
            return m_pools[get_type_id<T>().name].acquire();
        }

        template <typename T>
        void release(u32 id)
        {
            m_pools[get_type_id<T>().name].release(id);
        }

    private:
        using pool_t = handle_pool<u32, 0, handle_pool_policy::lifo>;

        std::unordered_map<hashed_string_view, pool_t, hash<hashed_string_view>> m_pools;
    };
}