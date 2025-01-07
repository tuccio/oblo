#include <oblo/options/options_manager.hpp>

#include <oblo/core/buffered_array.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/dynamic_array.hpp>

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
        };

        static constexpr h32<options_layer> g_defaultValuesLayer = h32<options_layer>{1u};
    }

    struct options_manager::impl
    {
        dynamic_array<layer_data> layers;
        deque<option_data> options;
        deque<option_layer_value> values;
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
            return {};
        }

        if (desc.kind == property_kind::string)
        {
            // Strings are not supported yet (we need to change the storage for those)
            OBLO_ASSERT(desc.kind != property_kind::string);
            return {};
        }

        // We allocate space for all layers the -1 is for the invalid layer
        const auto numLayers = m_impl->layers.size() - 1;

        const auto optIdx = narrow_cast<u32>(m_impl->options.size());

        auto& opt = m_impl->options.emplace_back();

        opt.kind = desc.kind;
        opt.layerValueBeginIdx = narrow_cast<u32>(m_impl->values.size());
        opt.layerValueEndIdx = narrow_cast<u32>(opt.layerValueBeginIdx + numLayers);

        m_impl->values.resize(m_impl->values.size() + numLayers);

        auto& defaultValue = m_impl->values[opt.layerValueBeginIdx];
        defaultValue.value = std::move(desc.defaultValue);

        return h32<option>{optIdx};
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
}