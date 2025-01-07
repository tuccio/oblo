#include <oblo/options/options_manager.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/iterator/iterator_range.hpp>
#include <oblo/core/string/string.hpp>
#include <oblo/properties/serialization/data_document.hpp>

#include <unordered_map>

namespace oblo
{
    namespace
    {
        struct layer_data
        {
            uuid id;
        };

        struct option_layer_value
        {
            property_value_wrapper value;
        };

        struct option_data
        {
            property_kind kind;
            u32 layerValueBeginIdx;
            u32 layerValueEndIdx;
            uuid id;
            string name;
            string category;
        };

        static constexpr h32<options_layer> g_defaultValuesLayer = h32<options_layer>{1u};
    }

    struct options_manager::impl
    {
        dynamic_array<layer_data> layers;
        deque<option_data> options;
        deque<option_layer_value> values;

        std::unordered_map<uuid, h32<option>> optionsMap;
    };

    options_manager::options_manager() = default;

    options_manager::options_manager(options_manager&&) noexcept = default;

    options_manager::~options_manager() = default;

    options_manager& options_manager::operator=(options_manager&&) noexcept = default;

    void options_manager::init(std::span<const options_layer_descriptor> layers)
    {
        m_impl = std::make_unique<impl>();

        auto& invalidOption = m_impl->options.emplace_back();
        invalidOption.kind = property_kind::enum_max;

        m_impl->layers.reserve(layers.size() + 2);

        auto& invalidLayer = m_impl->layers.emplace_back();
        invalidLayer.id = uuid{};

        auto& defaultValuesLayer = m_impl->layers.emplace_back();
        defaultValuesLayer.id = "714440ed-38e4-4e08-aaef-dc95fa378413"_uuid;

        for (auto& layer : layers)
        {
            m_impl->layers.emplace_back(layer.id);
        }

        m_impl->optionsMap.emplace(uuid{}, h32<option>{});
    }

    void options_manager::shutdown()
    {
        // TODO: Save if necessary
        m_impl.reset();
    }

    h32<options_layer> options_manager::get_default_layer() const
    {
        return g_defaultValuesLayer;
    }

    h32<options_layer> options_manager::find_layer(uuid id) const
    {
        h32<options_layer> h{};

        for (const auto& l : m_impl->layers)
        {
            if (l.id == id)
            {
                return h;
            }

            ++h.value;
        }

        return h32<options_layer>{};
    }

    h32<option> options_manager::register_option(const option_descriptor& desc)
    {
        OBLO_ASSERT(desc.defaultValue.get_kind() == desc.kind);
        OBLO_ASSERT(!desc.name.empty());

        if (desc.defaultValue.get_kind() != desc.kind || desc.kind == property_kind::enum_max)
        {
            OBLO_ASSERT(false, "Invalid default value");
            return {};
        }

        if (desc.kind == property_kind::string)
        {
            // Strings are not supported yet (we need to change the storage for those)
            OBLO_ASSERT(desc.kind != property_kind::string);
            return {};
        }

        const auto optIdx = narrow_cast<u32>(m_impl->options.size());

        const auto h = h32<option>{optIdx};

        const auto [it, ok] = m_impl->optionsMap.emplace(desc.id, h);

        if (!ok)
        {
            OBLO_ASSERT(false, "Options need unique ids");
            return {};
        }

        // We allocate space for all layers the -1 is for the invalid layer
        const auto numLayers = m_impl->layers.size() - 1;

        auto& opt = m_impl->options.emplace_back();

        opt.kind = desc.kind;
        opt.id = desc.id;
        opt.name = desc.name;
        opt.category = desc.category;
        opt.layerValueBeginIdx = narrow_cast<u32>(m_impl->values.size());
        opt.layerValueEndIdx = narrow_cast<u32>(opt.layerValueBeginIdx + numLayers);

        m_impl->values.resize(m_impl->values.size() + numLayers);

        auto& defaultValue = m_impl->values[opt.layerValueBeginIdx];
        defaultValue.value = std::move(desc.defaultValue);

        return h;
    }

    h32<option> options_manager::find_option(uuid id) const
    {
        const auto it = m_impl->optionsMap.find(id);

        if (it == m_impl->optionsMap.end())
        {
            return {};
        }

        return it->second;
    }

    expected<> options_manager::set_option_value(
        h32<options_layer> layer, h32<option> option, property_value_wrapper value)
    {
        OBLO_ASSERT(layer > g_defaultValuesLayer);

        // You cannot write the default value here
        if (layer <= g_defaultValuesLayer || layer.value >= m_impl->layers.size())
        {
            return unspecified_error;
        }

        if (option.value >= m_impl->options.size())
        {
            return unspecified_error;
        }

        auto& opt = m_impl->options[option.value];

        if (opt.kind != value.get_kind())
        {
            return unspecified_error;
        }

        // Locate the value, the -1 is for the invalid layer
        const u32 idx = opt.layerValueBeginIdx + layer.value - 1;
        m_impl->values[idx].value = std::move(value);

        return no_error;
    }

    expected<property_value_wrapper> options_manager::get_option_value(h32<options_layer> layer,
        h32<option> option) const
    {
        OBLO_ASSERT(layer > g_defaultValuesLayer);

        // You can read the default value here
        if (layer < g_defaultValuesLayer || layer.value >= m_impl->layers.size())
        {
            return unspecified_error;
        }

        if (option.value >= m_impl->options.size())
        {
            return unspecified_error;
        }

        auto& opt = m_impl->options[option.value];

        // Locate the value
        const u32 idx = opt.layerValueBeginIdx + layer.value - 1;

        for (u32 i = idx; i > opt.layerValueBeginIdx; --i)
        {
            auto& v = m_impl->values[i];

            if (v.value)
            {
                return v.value;
            }
        }

        return m_impl->values[opt.layerValueBeginIdx].value;
    }

    expected<> options_manager::clear_option_value(h32<options_layer> layer, h32<option> option)
    {
        OBLO_ASSERT(layer > g_defaultValuesLayer);

        // You cannot write the default value here
        if (layer <= g_defaultValuesLayer || layer.value >= m_impl->layers.size())
        {
            return unspecified_error;
        }

        if (option.value >= m_impl->options.size())
        {
            return unspecified_error;
        }

        auto& opt = m_impl->options[option.value];

        // Locate the value, the -1 is for the invalid layer
        const u32 idx = opt.layerValueBeginIdx + layer.value - 1;
        m_impl->values[idx].value = {};

        return no_error;
    }

    void options_manager::store_layer(data_document& doc, u32 root, h32<options_layer> layer) const
    {
        doc.make_array(root);

        for (const auto& opt : iterator_range{m_impl->options.begin() + 1, m_impl->options.end()})
        {
            // Locate the value, the -1 is for the invalid layer
            const u32 idx = opt.layerValueBeginIdx + layer.value - 1;

            const auto& v = m_impl->values[idx].value;

            if (v)
            {

                const auto n = doc.array_push_back(root);
                doc.make_object(n);

                doc.child_value(n, "name"_hsv, property_value_wrapper{opt.name});
                doc.child_value(n, "id"_hsv, property_value_wrapper{opt.id});
                doc.child_value(n, "value"_hsv, v);
            }
        }
    }

    void options_manager::load_layer(const data_document& doc, u32 root, h32<options_layer> layer)
    {
        OBLO_ASSERT(doc.is_array(root));

        // Clear all values
        for (const auto& opt : iterator_range{m_impl->options.begin() + 1, m_impl->options.end()})
        {
            // Locate the value, the -1 is for the invalid layer
            const u32 idx = opt.layerValueBeginIdx + layer.value - 1;

            m_impl->values[idx].value = {};
        }

        for (u32 child = doc.child_next(root, data_node::Invalid); child != data_node::Invalid;
             child = doc.child_next(root, child))
        {
            const auto idNode = doc.find_child(child, "id"_hsv);
            const auto valueNode = doc.find_child(child, "value"_hsv);

            if (idNode == data_node::Invalid || valueNode == data_node::Invalid)
            {
                continue;
            }

            uuid id{};

            if (const auto r = doc.read_uuid(idNode))
            {
                id = *r;
            }

            const auto it = m_impl->optionsMap.find(id);

            if (id == uuid{} || it == m_impl->optionsMap.end())
            {
                continue;
            }

            const auto& opt = m_impl->options[it->second.value];

            property_value_wrapper value{};

            switch (opt.kind)
            {
            case property_kind::boolean:
                if (auto r = doc.read_bool(valueNode))
                {
                    value = property_value_wrapper{*r};
                }
                break;

            case property_kind::f32:
                if (auto r = doc.read_f32(valueNode))
                {
                    value = property_value_wrapper{*r};
                }
                break;

            case property_kind::u32:
                if (auto r = doc.read_u32(valueNode))
                {
                    value = property_value_wrapper{*r};
                }
                break;

            default:
                OBLO_ASSERT(false, "Unsupported");
                break;
            }

            if (value)
            {
                OBLO_ASSERT(value.get_kind() == opt.kind);

                // Locate the value, the -1 is for the invalid layer
                const u32 idx = opt.layerValueBeginIdx + layer.value - 1;

                m_impl->values[idx].value = value;
            }
        }
    }
}