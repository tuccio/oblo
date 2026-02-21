#include <oblo/renderer/renderer_module.hpp>

#include <oblo/core/service_registry.hpp>
#include <oblo/core/types.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/options/option_proxy.hpp>
#include <oblo/options/option_traits.hpp>
#include <oblo/options/options_module.hpp>
#include <oblo/options/options_provider.hpp>
#include <oblo/reflection/codegen/registration.hpp>
#include <oblo/renderer/compiler/compiler_module.hpp>
#include <oblo/renderer/draw/global_shader_options.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
    struct tag_type_tag;
}

namespace oblo
{
    renderer_module::renderer_module() = default;

    renderer_module::~renderer_module() = default;

    bool renderer_module::startup(const module_initializer& initializer)
    {
        module_manager::get().load<options_module>();
        module_manager::get().load<compiler_module>();

        option_proxy_struct<global_shader_options_proxy>::register_options(*initializer.services);

        reflection::gen::load_module_and_register();

        return true;
    }

    void renderer_module::shutdown() {}

    bool renderer_module::finalize()
    {
        return true;
    }
}