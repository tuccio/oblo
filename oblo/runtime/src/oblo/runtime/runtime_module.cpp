#include <oblo/runtime/runtime_module.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/graphics/graphics_module.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/runtime/runtime_registry.hpp>
#include <oblo/scene/scene_module.hpp>

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

    bool runtime_module::startup(const module_initializer&)
    {
        OBLO_ASSERT(!g_instance);
        OBLO_ASSERT(!m_impl);

        m_impl = std::make_unique<impl>();

        auto& mm = module_manager::get();

        mm.load<graphics_module>();
        mm.load<scene_module>();

        auto* reflection = mm.load<reflection::reflection_module>();

        m_impl->propertyRegistry.init(reflection->get_registry());

        g_instance = this;
        return true;
    }

    void runtime_module::shutdown()
    {
        OBLO_ASSERT(g_instance == this);
        g_instance = nullptr;
        m_impl.reset();
    }

    runtime_registry runtime_module::create_runtime_registry() const
    {
        return runtime_registry{&m_impl->propertyRegistry};
    }
}