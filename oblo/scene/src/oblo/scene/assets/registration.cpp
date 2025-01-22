#include <oblo/scene/assets/registration.hpp>

#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_document.hpp>
#include <oblo/properties/serialization/json.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/texture.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>

#include <fstream>

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

        template <typename T>
        struct meta;

        template <>
        struct meta<model>
        {
            static bool save(const model& model, cstring_view destination)
            {
                data_document doc;
                doc.init();

                write_ref_array(doc, doc.get_root(), "meshes"_hsv, model.meshes);
                write_ref_array(doc, doc.get_root(), "materials"_hsv, model.materials);

                return json::write(doc, destination).has_value();
            }

            static bool load(model& model, cstring_view source)
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
        };

        template <>
        struct meta<mesh>
        {
            static bool save(const mesh& mesh, cstring_view destination)
            {
                return save_mesh(mesh, destination);
            }

            static bool load(mesh& mesh, cstring_view source)
            {
                return load_mesh(mesh, source);
            }
        };

        template <>
        struct meta<texture>
        {
            static bool save(const texture& texture, cstring_view destination)
            {
                return texture.save(destination);
            }

            static bool load(texture& texture, cstring_view source)
            {
                return texture.load(source);
            }
        };

        template <>
        struct meta<material>
        {
            static bool save(const material& material, cstring_view destination)
            {
                return material.save(destination);
            }

            static bool load(material& material, cstring_view source)
            {
                return material.load(source);
            }
        };

        template <typename T>
        resource_type_descriptor make_resource_type_desc()
        {
            return {
                .type = get_type_id<T>(),
                .create = []() -> void* { return new T{}; },
                .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
                .load = [](void* ptr, cstring_view source) { return meta<T>::load(*static_cast<T*>(ptr), source); },
                .save = [](const void* ptr, cstring_view destination)
                { return meta<T>::save(*static_cast<const T*>(ptr), destination); },
            };
        }
    }

    void fetch_scene_resource_types(deque<resource_type_descriptor>& outResourceTypes)
    {
        outResourceTypes.push_back(make_resource_type_desc<material>());
        outResourceTypes.push_back(make_resource_type_desc<mesh>());
        outResourceTypes.push_back(make_resource_type_desc<model>());
        outResourceTypes.push_back(make_resource_type_desc<texture>());
    }
}