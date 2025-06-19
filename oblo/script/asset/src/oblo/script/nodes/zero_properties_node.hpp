#pragma once

#include <oblo/nodes/node_interface.hpp>

namespace oblo
{
    class zero_properties_node : public node_interface
    {
    public:
        void fill_properties_schema(data_document&, u32) const override {}

        void store_properties(data_document&, u32) const override {}

        void load_properties(const data_document&, u32) override {}
    };
}