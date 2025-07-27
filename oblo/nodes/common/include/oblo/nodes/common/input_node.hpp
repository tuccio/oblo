#pragma once

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/nodes/node_interface.hpp>

namespace oblo
{
    class input_node : public node_interface
    {
    public:
        static constexpr uuid id = "d7bb0a4d-6598-4fb7-b4a7-2e014a82979f"_uuid;
        static constexpr cstring_view name = "Input";

    public:
        void on_create(const node_graph_context& g);
        void on_input_change(const node_graph_context& g);

        void fetch_properties_descriptors(dynamic_array<node_property_descriptor>& outDescriptors) const override;

        void store(data_document&, u32) const override;
        void load(const data_document&, u32) override;

        void set_input_id(const uuid& inputId);
        const uuid& get_input_id() const;

        bool generate(const node_graph_context& g,
            abstract_syntax_tree& ast,
            h32<ast_node> parent,
            const std::span<const h32<ast_node>> inputs,
            dynamic_array<h32<ast_node>>& outputs) const override;

    private:
        uuid m_inputId{};
    };
}