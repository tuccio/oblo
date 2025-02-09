#include <oblo/vulkan/renderer_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/core/types.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/reflection/registration/module_registration.hpp>
#include <oblo/scene/reflection/gpu_component.hpp>
#include <oblo/vulkan/compiler/compiler_module.hpp>
#include <oblo/vulkan/data/components.hpp>
#include <oblo/vulkan/data/tags_internal.hpp>
#include <oblo/vulkan/draw/global_shader_options.hpp>
#include <oblo/vulkan/draw/instance_data_type_registry.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
    struct tag_type_tag;
}

namespace oblo::vk
{
    namespace
    {
        void register_reflection(reflection::reflection_registry::registrant reg)
        {
            reg.add_class<draw_instance_component>()
                .add_ranged_type_erasure()
                .add_concept(gpu_component{"i_MeshHandles"})
                .add_tag<ecs::component_type_tag>();

            reg.add_class<draw_instance_id_component>().add_ranged_type_erasure().add_tag<ecs::component_type_tag>();
            reg.add_class<draw_mesh_component>().add_ranged_type_erasure().add_tag<ecs::component_type_tag>();
            reg.add_class<draw_raytraced_tag>().add_ranged_type_erasure().add_tag<ecs::tag_type_tag>();
            reg.add_class<mesh_index_none_tag>().add_ranged_type_erasure().add_tag<ecs::tag_type_tag>();
            reg.add_class<mesh_index_u8_tag>().add_ranged_type_erasure().add_tag<ecs::tag_type_tag>();
            reg.add_class<mesh_index_u16_tag>().add_ranged_type_erasure().add_tag<ecs::tag_type_tag>();
            reg.add_class<mesh_index_u32_tag>().add_ranged_type_erasure().add_tag<ecs::tag_type_tag>();
        }
    }

    renderer_module::renderer_module() = default;

    renderer_module::~renderer_module() = default;

    bool renderer_module::startup(const module_initializer& initializer)
    {
        module_manager::get().load<options_module>();
        module_manager::get().load<compiler_module>();

        option_proxy_struct<global_shader_options_proxy>::register_options(*initializer.services);

        reflection::load_module_and_register(register_reflection);

        return true;
    }

    void renderer_module::shutdown() {}

    void renderer_module::finalize() {}
}