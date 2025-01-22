#include <oblo/scene/assets/registration.hpp>

#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/assets/mesh.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/texture.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/scene/serialization/model_file.hpp>

#include <fstream>

namespace oblo
{
    namespace
    {
        template <typename>
        struct meta;

        template <>
        struct meta<model>
        {
            static bool load(model& model, cstring_view source)
            {
                return load_model(model, source);
            }
        };

        template <>
        struct meta<mesh>
        {
            static bool load(mesh& mesh, cstring_view source)
            {
                return load_mesh(mesh, source);
            }
        };

        template <>
        struct meta<texture>
        {
            static bool load(texture& texture, cstring_view source)
            {
                return texture.load(source);
            }
        };

        template <>
        struct meta<material>
        {

            static bool load(material& material, cstring_view source)
            {
                return material.load(source);
            }
        };
    }

    template <typename T>
    resource_type_descriptor make_resource_type_desc()
    {
        return {
            .type = get_type_id<T>(),
            .create = []() -> void* { return new T{}; },
            .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
            .load = [](void* ptr, cstring_view source) { return meta<T>::load(*static_cast<T*>(ptr), source); },
        };
    }

    void fetch_scene_resource_types(deque<resource_type_descriptor>& outResourceTypes)
    {
        outResourceTypes.push_back(make_resource_type_desc<material>());
        outResourceTypes.push_back(make_resource_type_desc<mesh>());
        outResourceTypes.push_back(make_resource_type_desc<model>());
        outResourceTypes.push_back(make_resource_type_desc<texture>());
    }
}