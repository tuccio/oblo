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

        void fill_properties_schema(data_document&, u32) const override;

        void store_properties(data_document&, u32) const override;
        void load_properties(const data_document&, u32) override;

        void set_input_id(const uuid& inputId);
        const uuid& get_input_id() const;

    private:
        uuid m_inputId{};
    };
}