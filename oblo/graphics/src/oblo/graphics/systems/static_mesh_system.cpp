#include <oblo/graphics/systems/static_mesh_system.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/math/vec3.hpp>
#include <oblo/resource/resource_ptr.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/material.hpp>
#include <oblo/scene/components/global_transform_component.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/draw/resource_cache.hpp>
#include <oblo/vulkan/renderer.hpp>

#include <span>

namespace oblo
{
    namespace
    {
        struct gpu_material
        {
            vec3 albedo;
            f32 padding;
        };

        gpu_material convert(resource_ptr<material> m)
        {
            gpu_material out{};

            if (m)
            {
                if (auto* const albedo = m->get_property("Albedo"))
                {
                    out.albedo = albedo->as<vec3>().value_or({});
                }
            }

            return out;
        }
    }

    void static_mesh_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<vk::renderer>();
        OBLO_ASSERT(m_renderer);

        m_resourceRegistry = ctx.services->find<resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        m_resourceCache = ctx.services->find<vk::resource_cache>();
        OBLO_ASSERT(m_resourceCache);

        auto& drawRegistry = m_renderer->get_draw_registry();
        m_transformBuffer = drawRegistry.get_or_register({"i_TransformBuffer", sizeof(mat4), alignof(mat4)});
        m_materialsBuffer =
            drawRegistry.get_or_register({"i_MaterialBuffer", sizeof(gpu_material), alignof(gpu_material)});

        update(ctx);
    }

    void static_mesh_system::update(const ecs::system_update_context& ctx)
    {
        const h32<vk::draw_buffer> bufferNames[] = {m_transformBuffer, m_materialsBuffer};

        std::byte* buffersData[2];

        auto& drawRegistry = m_renderer->get_draw_registry();

        // TODO: (#7) Implement a way to create components while iterating, or deferring
        for (const auto [entities, meshComponents, globalTransforms] :
            ctx.entities->range<static_mesh_component, global_transform_component>())
        {
            for (auto&& [entity, meshComponent, globalTransform] :
                zip_range(entities, meshComponents, globalTransforms))
            {
                if (!meshComponent.instance)
                {
                    const auto mesh = drawRegistry.get_or_create_mesh(*m_resourceRegistry, meshComponent.mesh);

                    if (!mesh)
                    {
                        continue;
                    }

                    const auto instance = drawRegistry.create_instance(mesh, bufferNames, buffersData);
                    meshComponent.instance = instance.value;

                    new (buffersData[0]) mat4{transpose(globalTransform.value)};

                    const resource_ptr m = m_resourceRegistry->get_resource(meshComponent.material.id).as<material>();

                    new (buffersData[1]) gpu_material{convert(m)};
                }
                else
                {
                    std::byte* transformData{};

                    drawRegistry.get_instance_data(h32<vk::draw_instance>{meshComponent.instance},
                        {&m_transformBuffer, 1},
                        {&transformData, 1});

                    new (transformData) mat4{transpose(globalTransform.value)};
                }
            }
        }
    }
}