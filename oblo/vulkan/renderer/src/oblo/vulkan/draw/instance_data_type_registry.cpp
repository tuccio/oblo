#include <oblo/vulkan/draw/instance_data_type_registry.hpp>

#include <oblo/core/string/string_builder.hpp>
#include <oblo/modules/module_manager.hpp>
#include <oblo/reflection/concepts/gpu_component.hpp>
#include <oblo/reflection/reflection_module.hpp>
#include <oblo/reflection/reflection_registry.hpp>

namespace oblo::vk
{
    namespace
    {
        void register_gpu_components(const reflection::reflection_registry& reflection,
            instance_data_type_registry& instanceDataRegistry)
        {
            deque<reflection::type_handle> gpuComponentTypes;
            reflection.find_by_concept<reflection::gpu_component>(gpuComponentTypes);

            for (const auto typeHandle : gpuComponentTypes)
            {
                const auto typeData = reflection.get_type_data(typeHandle);

                const auto gpuComponent = reflection.find_concept<reflection::gpu_component>(typeHandle);
                instanceDataRegistry.register_instance_data(typeData.type, gpuComponent->bufferName);
            }
        }
    }

    void instance_data_type_registry::register_from_module()
    {
        auto* const rm = module_manager::get().find<reflection::reflection_module>();

        if (rm)
        {
            register_gpu_components(rm->get_registry(), *this);
        }
    }

    void instance_data_type_registry::register_instance_data(type_id type, hashed_string_view name)
    {
        const u32 gpuId = narrow_cast<u32>(m_instanceDataTypeNames.size());
        m_instanceDataTypeNames.emplace(type, instance_data_type_info{name, gpuId});
    }

    void instance_data_type_registry::generate_defines(string_builder& builder) const
    {
        constexpr auto nameLengthGuesstimate = 64;
        constexpr auto sizePerLine = sizeof("#define OBLO_INSTANCE_DATA_ 99\n") + nameLengthGuesstimate;

        builder.reserve(builder.size() + sizePerLine * m_instanceDataTypeNames.size());

        for (auto& [_, info] : m_instanceDataTypeNames)
        {
            builder.format("#define OBLO_INSTANCE_DATA_{} {}\n", info.bufferName, info.gpuBufferId);
        }
    }

    const instance_data_type_info* instance_data_type_registry::try_find(type_id type) const
    {
        const auto it = m_instanceDataTypeNames.find(type);
        return it == m_instanceDataTypeNames.end() ? nullptr : &it->second;
    }
}