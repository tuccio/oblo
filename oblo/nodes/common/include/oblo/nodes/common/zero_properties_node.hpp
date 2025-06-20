#pragma once

#include <oblo/nodes/node_interface.hpp>

namespace oblo
{
    class zero_properties_node : public node_interface
    {
    public:
        void fetch_properties_descriptors(dynamic_array<node_property_descriptor>&) const override {}

        void store(data_document&, u32) const override {}

        void load(const data_document&, u32) override {}
    };
}