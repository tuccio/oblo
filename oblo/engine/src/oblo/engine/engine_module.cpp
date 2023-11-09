#include <oblo/engine/engine_module.hpp>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/core/debug.hpp>
#include <oblo/engine/components/global_transform_component.hpp>
#include <oblo/engine/components/name_component.hpp>
#include <oblo/engine/components/position_component.hpp>
#include <oblo/engine/components/rotation_component.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/properties/property_registry.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/registration/registrant.hpp>
#include <oblo/resource/resource_registry.hpp>

namespace oblo::ecs
{
    struct component_type_tag;
}

namespace oblo
{
    namespace
    {
        engine_module* g_instance{};

        void register_reflection(reflection::reflection_registry::registrant reg)
        {
            reg.add_class<name_component>()
                .add_field(&name_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<position_component>()
                .add_field(&position_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<rotation_component>()
                .add_field(&rotation_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();

            reg.add_class<global_transform_component>()
                .add_field(&global_transform_component::value, "value")
                .add_ranged_type_erasure()
                .add_tag<ecs::component_type_tag>();
        }
    }

    engine_module::engine_module() = default;

    engine_module::~engine_module() = default;

    bool engine_module::startup()
    {
        OBLO_ASSERT(!g_instance);

        auto& mm = module_manager::get();

        auto* reflection = mm.load<reflection::reflection_module>();
        register_reflection(reflection->get_registrant());

        m_assetRegistry = std::make_unique<asset_registry>();

        // TODO: Load a project instead
        if (!m_assetRegistry->initialize("./project/assets", "./project/artifacts", "./project/sources"))
        {
            return false;
        }

        m_resourceRegistry = std::make_unique<resource_registry>();
        m_resourceRegistry->register_provider(&asset_registry::find_artifact_resource, m_assetRegistry.get());
        m_propertyRegistry = std::make_unique<property_registry>();
        m_propertyRegistry->init(reflection->get_registry());

        g_instance = this;
        return true;
    }

    void engine_module::shutdown()
    {
        OBLO_ASSERT(g_instance == this);

        m_resourceRegistry.reset();
        m_assetRegistry.reset();
        m_propertyRegistry.reset();
    }
}