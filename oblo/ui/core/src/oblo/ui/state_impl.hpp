#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/ui/state.hpp>

namespace oblo::ui
{
    enum class element_kind : u8
    {
        
    };

    struct element_data
    {
    };

    class elements_map
    {
    public:
        void clear()
        {
            m_ids.clear();
            m_elements.clear();
        }

        u32 add_element_with_id(id);
        u32 add_element_no_id();

    private:
        struct element_with_id
        {
            id id;
            u32 index;
        };

    private:
        dynamic_array<element_with_id> m_ids;
        dynamic_array<element_data> m_elements;
    };

    struct state
    {
        static constexpr u32 num_buffers = 2;

        elements_map elements[num_buffers];
        u8 currentBufferIdx{};
    };
}