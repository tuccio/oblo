#include <oblo/scene/assets/material.hpp>

#include <oblo/core/overload.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/properties/serialization/visit.hpp>

namespace oblo
{
    void material::set_property(std::string name, const type_id& type, const material_data_storage& value)
    {
        const auto it = m_map.find(name);

        if (it == m_map.end())
        {
            const auto [it, ok] = m_map.emplace(std::move(name), m_properties.size());
            m_properties.emplace_back(it->first, type, value);
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

    bool material::save(const property_registry& registry, const std::filesystem::path& destination) const
    {
        data_document doc;

        doc.init();

        const u32 root = doc.get_root();

        for (const auto& property : m_properties)
        {
            const auto kind = registry.find_property_kind(property.type);

            if (kind == property_kind::enum_max)
            {
                continue;
            }

            doc.child_value(root, property.name, kind, std::span{property.storage.buffer});
        }

        return json::write(doc, destination);
    }

    bool material::load(const property_registry& registry, const std::filesystem::path& source)
    {
        (void) registry;

        m_map.clear();
        m_properties.clear();

        data_document doc;

        if (!json::read(doc, source))
        {
            return false;
        }

        visit(doc,
            overload{[](const std::string_view, data_node_object_start) { return visit_result::recurse; },
                [](const std::string_view, data_node_object_finish) { return visit_result::recurse; },
                [this](const std::string_view key, const void* value, property_kind kind, data_node_value)
                {
                    switch (kind)
                    {
                    case property_kind::boolean:
                        set_property(std::string{key}, *reinterpret_cast<const bool*>(value));
                        break;

                    case property_kind::f32:
                        set_property(std::string{key}, *reinterpret_cast<const f32*>(value));
                        break;

                    case property_kind::f64:
                        set_property(std::string{key}, f32(*reinterpret_cast<const f64*>(value)));
                        break;

                    case property_kind::i32:
                        set_property(std::string{key}, *reinterpret_cast<const i32*>(value));
                        break;

                    case property_kind::u32:
                        set_property(std::string{key}, *reinterpret_cast<const u32*>(value));
                        break;

                    case property_kind::i64:
                        set_property(std::string{key}, *reinterpret_cast<const i64*>(value));
                        break;

                    case property_kind::u64:
                        set_property(std::string{key}, *reinterpret_cast<const u64*>(value));
                        break;

                    default:
                        return visit_result::recurse;
                    }

                    return visit_result::recurse;
                }});

        return true;
    }
}