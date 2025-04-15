#include <oblo/runtime/runtime_module.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/service_registry.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/modules/module_initializer.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/scene/scene_module.hpp>
#include <oblo/scene/utility/ecs_utility.hpp>

#include <module_loader_core.gen.hpp>

namespace oblo
{
    namespace
    {
        runtime_module* g_instance{};
    }

    struct runtime_module::impl
    {
        property_registry propertyRegistry;
    };

    runtime_module& runtime_module::get()
    {
        return *g_instance;
    }

    runtime_module::runtime_module() = default;

    runtime_module::~runtime_module() = default;

    bool runtime_module::startup(const module_initializer& initializer)
    {
        OBLO_ASSERT(!g_instance);
        OBLO_ASSERT(!m_impl);

        m_impl = allocate_unique<impl>();

        gen::load_modules_core();

        auto& mm = module_manager::get();

        mm.load<graphics_module>();
        mm.load<scene_module>();

        auto* reflection = mm.load<reflection::reflection_module>();

        m_impl->propertyRegistry.init(reflection->get_registry());

        initializer.services->add<const reflection::reflection_registry>().externally_owned(
            &reflection->get_registry());

        initializer.services->add<const property_registry>().externally_owned(&m_impl->propertyRegistry);

        g_instance = this;
        return true;
    }

    void runtime_module::shutdown()
    {
        OBLO_ASSERT(g_instance == this);
        g_instance = nullptr;
        m_impl.reset();
    }

    bool runtime_module::finalize()
    {
        auto& mm = module_manager::get();
        auto* reflection = mm.find<reflection::reflection_module>();

        ecs_utility::register_reflected_component_and_tag_types(reflection->get_registry(),
            nullptr,
            &m_impl->propertyRegistry);

        return true;
    }

    runtime_registry runtime_module::create_runtime_registry() const
    {
        return runtime_registry{&m_impl->propertyRegistry};
    }
}