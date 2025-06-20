#include <oblo/nodes/common/input_node.hpp>

#include <oblo/properties/property_value_wrapper.hpp>
#include <oblo/properties/serialization/data_document.hpp>

namespace oblo
{
    void input_node::on_create(const node_graph_context&) {}
    void input_node::on_input_change(const node_graph_context&) {}

    void input_node::fetch_properties_descriptors(dynamic_array<node_property_descriptor>&) const {}

    void input_node::store(data_document& doc, u32 nodeIndex) const
    {
        doc.child_value(nodeIndex, "id"_hsv, property_value_wrapper{m_inputId});
    }

    void input_node::load(const data_document& doc, u32 nodeIndex)
    {
        const u32 child = doc.find_child(nodeIndex, "id"_hsv);
        m_inputId = doc.read_uuid(child).value_or(uuid{});
    }

    void input_node::set_input_id(const uuid& inputId)
    {
        m_inputId = inputId;
    }

    const uuid& input_node::get_input_id() const
    {
        return m_inputId;
    }
}