#include <oblo/scene/assets/material.hpp>

#include <oblo/core/overload.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/math/vec2.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/math/vec4.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/properties/serialization/visit.hpp>
#include <oblo/resource/resource_ref.hpp>

namespace oblo
{
    void material::set_property(std::string_view name, material_property_type type, const material_data_storage& value)
    {
        const auto it = m_map.find(name);

        if (it == m_map.end())
        {
            const auto [it, ok] = m_map.emplace(std::string{name}, m_properties.size());
            m_properties.emplace_back(it->first, type, value);
            return;
        }

        auto& p = m_properties[it->second];
        p.type = type;
        p.storage = value;
    }

    const material_property* material::get_property(const std::string_view name) const
    {
        const auto it = m_map.find(name);

        if (it == m_map.end())
        {
            return nullptr;
        }

        return &m_properties[it->second];
    }

    std::span<const material_property> material::get_properties() const
    {
        return m_properties;
    }

    bool material::save(const std::filesystem::path& destination) const
    {
        data_document doc;

        doc.init();

        const u32 root = doc.get_root();

        for (const auto& property : m_properties)
        {
            const auto propertyNode = doc.child_object(root, property.name);

            doc.child_value(propertyNode, "type", property_kind::u8, std::as_bytes(std::span{&property.type, 1}));

            constexpr std::string_view valueLabel{"value"};

            switch (property.type)
            {
            case material_property_type::f32:
                doc.child_value(propertyNode, valueLabel, property_kind::f32, std::span{property.storage.buffer});
                break;

            case material_property_type::vec2: {
                auto* const v = reinterpret_cast<const float*>(property.storage.buffer);

                const auto valueNode = doc.child_object(propertyNode, valueLabel);

                doc.child_value(valueNode, "x", property_kind::f32, std::as_bytes(std::span{v + 0, 1}));
                doc.child_value(valueNode, "y", property_kind::f32, std::as_bytes(std::span{v + 1, 1}));
            }
            break;

            case material_property_type::vec3: {
                auto* const v = reinterpret_cast<const float*>(property.storage.buffer);

                const auto valueNode = doc.child_object(propertyNode, valueLabel);

                doc.child_value(valueNode, "x", property_kind::f32, std::as_bytes(std::span{v + 0, 1}));
                doc.child_value(valueNode, "y", property_kind::f32, std::as_bytes(std::span{v + 1, 1}));
                doc.child_value(valueNode, "z", property_kind::f32, std::as_bytes(std::span{v + 2, 1}));
            }
            break;

            case material_property_type::vec4: {
                auto* const v = reinterpret_cast<const float*>(property.storage.buffer);

                const auto valueNode = doc.child_object(propertyNode, valueLabel);

                doc.child_value(valueNode, "x", property_kind::f32, std::as_bytes(std::span{v + 0, 1}));
                doc.child_value(valueNode, "y", property_kind::f32, std::as_bytes(std::span{v + 1, 1}));
                doc.child_value(valueNode, "z", property_kind::f32, std::as_bytes(std::span{v + 2, 1}));
                doc.child_value(valueNode, "w", property_kind::f32, std::as_bytes(std::span{v + 3, 1}));
            }
            break;

            case material_property_type::texture:
                doc.child_value(propertyNode, valueLabel, property_kind::uuid, std::span{property.storage.buffer});
                break;
            }
        }

        return json::write(doc, destination);
    }

    bool material::load(const std::filesystem::path& source)
    {
        m_map.clear();
        m_properties.clear();

        data_document doc;

        if (!json::read(doc, source))
        {
            return false;
        }

        const std::span nodes = doc.get_nodes();
        const auto root = doc.get_root();

        for (u32 index = nodes[root].object.firstChild; index != data_node::Invalid; index = nodes[index].nextSibling)
        {
            if (nodes[index].kind != data_node_kind::object)
            {
                continue;
            }

            const u32 typeIndex = doc.find_child(index, "type");
            const u32 valueIndex = doc.find_child(index, "value");

            if (typeIndex == data_node::Invalid || valueIndex == data_node::Invalid)
            {
                continue;
            }

            // TODO: Check if type matches (should do the same in the switch below)
            const auto type = material_property_type(*reinterpret_cast<const u32*>(nodes[typeIndex].value.data));
            const auto key = doc.get_node_name(index);

            constexpr auto any_invalid = [](auto... n) { return ((n == data_node::Invalid) || ...); };

            auto* const value = nodes[valueIndex].value.data;

            switch (type)
            {
            case material_property_type::f32:
                set_property(std::string{key}, doc.read_f32(valueIndex).value_or(0.f));
                break;

            case material_property_type::vec2: {
                const auto x = doc.find_child(valueIndex, "x");
                const auto y = doc.find_child(valueIndex, "y");

                if (any_invalid(x, y))
                {
                    break;
                }

                const vec2 v{
                    doc.read_f32(x).value_or(0.f),
                    doc.read_f32(y).value_or(0.f),
                };

                set_property(std::string{key}, v);
            }
            break;

            case material_property_type::vec3: {
                const auto x = doc.find_child(valueIndex, "x");
                const auto y = doc.find_child(valueIndex, "y");
                const auto z = doc.find_child(valueIndex, "z");

                if (any_invalid(x, y, z))
                {
                    break;
                }

                const vec3 v{
                    doc.read_f32(x).value_or(0.f),
                    doc.read_f32(y).value_or(0.f),
                    doc.read_f32(z).value_or(0.f),
                };

                set_property(std::string{key}, v);
            }
            break;

            case material_property_type::vec4: {
                const auto x = doc.find_child(valueIndex, "x");
                const auto y = doc.find_child(valueIndex, "y");
                const auto z = doc.find_child(valueIndex, "z");
                const auto w = doc.find_child(valueIndex, "w");

                if (any_invalid(x, y, z, w))
                {
                    break;
                }

                const vec4 v{
                    doc.read_f32(x).value_or(0.f),
                    doc.read_f32(y).value_or(0.f),
                    doc.read_f32(z).value_or(0.f),
                    doc.read_f32(w).value_or(0.f),
                };

                set_property(std::string{key}, v);
            }
            break;

            case material_property_type::texture: {
                resource_ref<texture> texture;

                if (texture.id.parse(*reinterpret_cast<const std::string_view*>(value)))
                {
                    set_property(std::string{key}, texture);
                }
            }
            break;
            }
        }

        return true;
    }
}