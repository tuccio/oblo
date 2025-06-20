#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>
#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>

namespace oblo
{
    template <node_primitive_kind Kind, typename T, typename Base>
    class constant_node_base : public node_interface
    {
    public:
        void on_create(const node_graph_context& g) override
        {
            g.add_out_pin({
                .id = "3168ae07-af54-4a01-a861-7c56d7d90418"_uuid,
                .name = "Value",
            });
        }

        void on_input_change(const node_graph_context&) override
        {
            OBLO_ASSERT(false, "This should not happen, we have no inputs");
        }

        void fill_properties_schema(data_document& doc, u32 nodeIndex) const override
        {
            doc.child_value(nodeIndex, "value"_hsv, property_value_wrapper{get_node_primitive_type_id<Kind>()});
        }

        void store_properties(data_document& doc, u32 nodeIndex) const override
        {
            doc.child_value(nodeIndex, "value"_hsv, property_value_wrapper{m_value});
        }

        void load_properties(const data_document& doc, u32 nodeIndex) override
        {
            const auto childIndex = doc.find_child(nodeIndex, "value"_hsv);
            const auto r = Base::read_value(doc, childIndex);

            if (r)
            {
                m_value = *r;
            }
        }

    private:
        T m_value{};
    };

    class bool_constant_node final : public constant_node_base<node_primitive_kind::boolean, bool, bool_constant_node>
    {
    public:
        static constexpr uuid id = "07a7955f-4aa9-4ae6-a781-9d8417755249"_uuid;
        static constexpr cstring_view name = "Bool Constant";

        static auto read_value(const data_document& doc, u32 nodeIndex)
        {
            return doc.read_bool(nodeIndex);
        }
    };

    class f32_constant_node final : public constant_node_base<node_primitive_kind::f32, f32, f32_constant_node>
    {
    public:
        static constexpr uuid id = "53b6e2bf-f0fc-43e3-ade4-25f3a74a42e1"_uuid;
        static constexpr cstring_view name = "F32 Constant";

        static auto read_value(const data_document& doc, u32 nodeIndex)
        {
            return doc.read_f32(nodeIndex);
        }
    };
}