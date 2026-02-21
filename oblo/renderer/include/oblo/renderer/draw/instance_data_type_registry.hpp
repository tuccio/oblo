#pragma once

#include <oblo/core/string/hashed_string_view.hpp>
#include <oblo/core/type_id.hpp>

#include <unordered_map>

namespace oblo
{
    class string_builder;
}

namespace oblo
{
    struct instance_data_type_info
    {
        hashed_string_view bufferName;
        u32 gpuBufferId;
    };

    class instance_data_type_registry
    {
    public:
        void register_from_module();
        void register_instance_data(type_id type, hashed_string_view name);

        void generate_defines(string_builder& builder) const;

        const instance_data_type_info* try_find(type_id type) const;

        auto begin() const
        {
            return m_instanceDataTypeNames.begin();
        }

        auto end() const
        {
            return m_instanceDataTypeNames.end();
        }

    private:
        std::unordered_map<type_id, instance_data_type_info, hash<type_id>> m_instanceDataTypeNames;
    };
}