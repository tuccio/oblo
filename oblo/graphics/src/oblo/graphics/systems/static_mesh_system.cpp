#include <oblo/graphics/systems/static_mesh_system.hpp>

#include <oblo/core/array_size.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/core/zip_range.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/ecs/systems/system_update_context.hpp>
#include <oblo/ecs/type_registry.hpp>
#include <oblo/ecs/utility/registration.hpp>
#include <oblo/engine/components/global_transform_component.hpp>
#include <oblo/graphics/components/static_mesh_component.hpp>
#include <oblo/vulkan/draw/draw_registry.hpp>
#include <oblo/vulkan/renderer.hpp>

#include <span>

namespace oblo
{
    void static_mesh_system::first_update(const ecs::system_update_context& ctx)
    {
        m_renderer = ctx.services->find<vk::renderer>();
        OBLO_ASSERT(m_renderer);

        m_resourceRegistry = ctx.services->find<resource_registry>();
        OBLO_ASSERT(m_resourceRegistry);

        auto& drawRegistry = m_renderer->get_draw_registry();
        m_transformBuffer = drawRegistry.get_or_register({"i_TransformBuffer", sizeof(mat4), alignof(mat4)});

        update(ctx);
    }

    void static_mesh_system::update(const ecs::system_update_context& ctx)
    {
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
                    std::byte* transformData{};

                    const auto mesh = drawRegistry.get_or_create_mesh(*m_resourceRegistry, meshComponent.mesh);
                    const auto instance =
                        drawRegistry.create_instance(mesh, {&m_transformBuffer, 1}, {&transformData, 1});
                    meshComponent.instance = instance.value;

                    new (transformData) mat4{transpose(globalTransform.value)};
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