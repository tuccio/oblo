#pragma once

#include <oblo/core/type_id.hpp>
#include <oblo/nodes/common/fundamental_types.hpp>
#include <oblo/nodes/node_descriptor.hpp>
#include <oblo/nodes/node_graph.hpp>
#include <oblo/nodes/node_interface.hpp>
#include <oblo/nodes/node_property_descriptor.hpp>
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
            const h32 out = g.add_out_pin({
                .id = "3168ae07-af54-4a01-a861-7c56d7d90418"_uuid,
                .name = "Value",
            });

            g.set_deduced_type(out, get_node_primitive_type_id<Kind>());
        }

        void on_input_change(const node_graph_context&) override
        {
            OBLO_ASSERT(false, "This should not happen, we have no inputs");
        }

        void fetch_properties_descriptors(dynamic_array<node_property_descriptor>& outDescriptors) const override
        {
            outDescriptors.push_back({
                .name = "value",
                .typeId = get_node_primitive_type_id<Kind>(),
            });
        }

        void store(data_document& doc, u32 nodeIndex) const override
        {
            doc.child_value(nodeIndex, "value"_hsv, property_value_wrapper{m_value});
        }

        void load(const data_document& doc, u32 nodeIndex) override
        {
            const auto childIndex = doc.find_child(nodeIndex, "value"_hsv);
            const auto r = Base::read_value(doc, childIndex);

            if (r)
            {
                m_value = *r;
            }
        }

    protected:
        T m_value{};
    };

    class bool_constant_node final : public constant_node_base<node_primitive_kind::boolean, bool, bool_constant_node>
    {
    public:
        static constexpr uuid id = "07a7955f-4aa9-4ae6-a781-9d8417755249"_uuid;
        static constexpr cstring_view name = "Boolean Constant";

        static auto read_value(const data_document& doc, u32 nodeIndex)
        {
            return doc.read_bool(nodeIndex);
        }

        bool generate(const node_graph_context&,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>>,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            const h32 out = ast.add_node(parent, ast_u32_constant{.value = u32{m_value}});
            outputs.emplace_back(out);
            return true;
        }
    };

    class i32_constant_node final : public constant_node_base<node_primitive_kind::i32, i32, i32_constant_node>
    {
    public:
        static constexpr uuid id = "e6b96480-a66f-4b08-83e8-235722d79a7a"_uuid;
        static constexpr cstring_view name = "Integer Constant";

        static expected<i32> read_value(const data_document& doc, u32 nodeIndex)
        {
            const expected v = doc.read_f64(nodeIndex);

            if (!v)
            {
                return unspecified_error;
            }

            return i32(*v);
        }

        bool generate(const node_graph_context&,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>>,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            const h32 out = ast.add_node(parent, ast_i32_constant{.value = m_value});
            outputs.emplace_back(out);
            return true;
        }
    };

    class f32_constant_node final : public constant_node_base<node_primitive_kind::f32, f32, f32_constant_node>
    {
    public:
        static constexpr uuid id = "53b6e2bf-f0fc-43e3-ade4-25f3a74a42e1"_uuid;
        static constexpr cstring_view name = "Float Constant";

        static auto read_value(const data_document& doc, u32 nodeIndex)
        {
            return doc.read_f32(nodeIndex);
        }

        bool generate(const node_graph_context&,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>>,
            dynamic_array<h32<ast_node>>& outputs) const override
        {
            const h32 out = ast.add_node(parent, ast_f32_constant{.value = m_value});
            outputs.emplace_back(out);
            return true;
        }
    };
}