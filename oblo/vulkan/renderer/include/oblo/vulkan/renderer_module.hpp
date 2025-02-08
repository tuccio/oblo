#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/unique_ptr.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::vk
{
    class instance_data_type_registry;
    struct required_features;

    class renderer_module final : public module_interface
    {
    public:
        renderer_module();
        renderer_module(const renderer_module&) = delete;
        renderer_module(renderer_module&&) noexcept = delete;
        ~renderer_module();

        renderer_module& operator=(const renderer_module&) = delete;
        renderer_module& operator=(renderer_module&&) noexcept = delete;

        bool startup(const module_initializer& initializer) override;
        void shutdown() override;
        void finalize() override;

        required_features get_required_features();

        bool is_ray_tracing_enabled() const;

        const instance_data_type_registry& get_instance_data_type_registry() const;

    private:
        unique_ptr<instance_data_type_registry> m_instanceDataTypeRegistry;
        dynamic_array<const char*> m_instanceExtensions;
        dynamic_array<const char*> m_deviceExtensions;
        void* m_deviceFeaturesChain{};
        bool m_withRayTracing{};
    };
}