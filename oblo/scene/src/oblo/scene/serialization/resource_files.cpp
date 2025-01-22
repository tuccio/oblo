#include <oblo/scene/serialization/model_file.hpp>

#include <oblo/core/string/cstring_view.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/scene/assets/model.hpp>

namespace oblo
{
    namespace
    {
        template <typename T>
        void write_ref_array(
            data_document& doc, u32 parent, hashed_string_view name, const dynamic_array<resource_ref<T>>& array)
        {
            const auto node = doc.child_array(parent, name);

            for (const auto& ref : array)
            {
                const auto v = doc.array_push_back(node);
                doc.make_value(v, property_kind::uuid, as_bytes(ref.id));
            }
        }

        template <typename T>
        bool read_ref_array(
            data_document& doc, u32 parent, hashed_string_view name, dynamic_array<resource_ref<T>>& array)
        {
            const auto node = doc.find_child(parent, name);

            if (node == data_node::Invalid || !doc.is_array(node))
            {
                return false;
            }

            for (u32 child = doc.child_next(node, data_node::Invalid); child != data_node::Invalid;
                 child = doc.child_next(node, child))
            {
                const uuid id = doc.read_uuid(child).value_or({});
                array.emplace_back(id);
            }

            return true;
        }
    }

    bool save_model_json(const model& model, cstring_view destination)
    {
        data_document doc;
        doc.init();

        write_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
        write_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

        return json::write(doc, destination).has_value();
    }

    bool load_model(model& model, cstring_view source)
    {
        data_document doc;

        if (!json::read(doc, source))
        {
            return false;
        }

        read_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
        read_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

        return true;
    }
}