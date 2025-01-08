#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/options/option_traits.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_value_wrapper.hpp>

#include <memory>
#include <span>

namespace oblo
{
    template <typename>
    class handle_range;

    class cstring_view;
    class data_document;
    struct options_layer;
    struct option;
    struct option_descriptor;

    struct options_layer_descriptor
    {
        uuid id;
    };

    class options_manager
    {
    public:
        options_manager();
        options_manager(const options_manager&) = delete;
        options_manager(options_manager&&) noexcept;
        ~options_manager();

        options_manager& operator=(const options_manager&) = delete;
        options_manager& operator=(options_manager&&) noexcept;

        void init(std::span<const options_layer_descriptor> layers);
        void shutdown();

        h32<options_layer> get_default_layer() const;
        h32<options_layer> get_highest_layer() const;

        h32<options_layer> find_layer(uuid id) const;

        u32 get_change_id(h32<options_layer> layer) const;

        handle_range<h32<option>> get_options_range() const;
        h32<option> register_option(const option_descriptor& desc);
        h32<option> find_option(uuid id) const;

        uuid get_option_uuid(h32<option> option) const;
        cstring_view get_option_name(h32<option> option) const;
        cstring_view get_option_category(h32<option> option) const;
        pair<property_value_wrapper, property_value_wrapper> get_option_value_ranges(h32<option> option) const;

        expected<> set_option_value(h32<options_layer> layer, h32<option> option, property_value_wrapper value);

        expected<property_value_wrapper> get_option_value(h32<options_layer> layer, h32<option> option) const;
        expected<property_value_wrapper> get_option_value(h32<option> option) const;

        expected<> clear_option_value(h32<options_layer> layer, h32<option> option);

        void store_layer(data_document& doc, u32 root, h32<options_layer> layer) const;
        void load_layer(const data_document& doc, u32 root, h32<options_layer> layer);

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };
}