#include <oblo/scene/resources/registration.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/resource/descriptors/resource_type_descriptor.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/resources/entity_hierarchy.hpp>
#include <oblo/scene/resources/material.hpp>
#include <oblo/scene/resources/mesh.hpp>
#include <oblo/scene/resources/model.hpp>
#include <oblo/scene/resources/texture.hpp>
#include <oblo/scene/resources/traits.hpp>
#include <oblo/scene/serialization/ecs_serializer.hpp>
#include <oblo/scene/serialization/entity_hierarchy_serialization_context.hpp>
#include <oblo/scene/serialization/mesh_file.hpp>
#include <oblo/scene/serialization/model_file.hpp>

namespace oblo
{
    namespace
    {
        template <typename>
        struct meta;

        template <>
        struct meta<entity_hierarchy>
        {
            static bool load(entity_hierarchy& hierarchy, cstring_view source, const any& ctx)
            {
                const auto& ehCtx = *ctx.as<entity_hierarchy_serialization_context>();
                return hierarchy.init(ehCtx.get_type_registry()).has_value() &&
                    hierarchy.load(source, ehCtx).has_value();
            }

            static any make_load_userdata()
            {
                any ctx;
                ctx.emplace<entity_hierarchy_serialization_context>().init().assert_value();
                return ctx;
            }
        };

        template <>
        struct meta<material>
        {
            static bool load(material& material, cstring_view source, const any&)
            {
                return material.load(source);
            }

            static any make_load_userdata()
            {
                return {};
            }
        };

        template <>
        struct meta<mesh>
        {
            static bool load(mesh& mesh, cstring_view source, const any&)
            {
                return load_mesh(mesh, source);
            }

            static any make_load_userdata()
            {
                return {};
            }
        };

        template <>
        struct meta<model>
        {
            static bool load(model& model, cstring_view source, const any&)
            {
                return load_model(model, source);
            }

            static any make_load_userdata()
            {
                return {};
            }
        };

        template <>
        struct meta<texture>
        {
            static bool load(texture& texture, cstring_view source, const any&)
            {
                return texture.load(source).has_value();
            }

            static any make_load_userdata()
            {
                return {};
            }
        };
    }

    template <typename T>
    resource_type_descriptor make_resource_type_desc()
    {
        return {
            .typeId = get_type_id<T>(),
            .typeUuid = resource_type<T>,
            .create = []() -> void* { return new T{}; },
            .destroy = [](void* ptr) { delete static_cast<T*>(ptr); },
            .load = [](void* ptr, cstring_view source, const any& ctx)
            { return meta<T>::load(*static_cast<T*>(ptr), source, ctx); },
            .userdata = meta<T>::make_load_userdata(),
        };
    }

    void fetch_scene_resource_types(deque<resource_type_descriptor>& outResourceTypes)
    {
        outResourceTypes.push_back(make_resource_type_desc<entity_hierarchy>());
        outResourceTypes.push_back(make_resource_type_desc<material>());
        outResourceTypes.push_back(make_resource_type_desc<mesh>());
        outResourceTypes.push_back(make_resource_type_desc<model>());
        outResourceTypes.push_back(make_resource_type_desc<texture>());
    }
}