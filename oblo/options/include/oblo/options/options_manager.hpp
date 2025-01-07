#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_value_wrapper.hpp>

#include <memory>
#include <span>

namespace oblo
{
    struct options_layer;
    struct option;

    struct option_descriptor
    {
        property_kind kind;
        string_view name;
        string_view category;
        property_value_wrapper defaultValue;
    };

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

        h32<options_layer> find_layer(uuid id) const;

        h32<option> register_option(const option_descriptor& desc);

        expected<> set_option_value(h32<options_layer> layer, h32<option> option, property_value_wrapper value);
        expected<property_value_wrapper> get_option_value(h32<options_layer> layer, h32<option> option) const;
        expected<> clear_option_value(h32<options_layer> layer, h32<option> option);

        expected<> save_layer_to_json(h32<options_layer> layer, string_view path) const;
        expected<> load_layer_from_json(h32<options_layer> layer, string_view path);

    private:
        struct impl;

    private:
        std::unique_ptr<impl> m_impl;
    };
}