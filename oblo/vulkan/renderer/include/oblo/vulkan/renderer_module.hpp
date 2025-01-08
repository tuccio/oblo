#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo::vk
{
    struct required_features;

    class renderer_module final : public module_interface
    {
    public:
        static renderer_module& get();

    public:
        bool startup(const module_initializer& initializer) override;
        void shutdown() override;

        required_features get_required_features();

        bool is_ray_tracing_enabled() const;

    private:
        dynamic_array<const char*> m_instanceExtensions;
        dynamic_array<const char*> m_deviceExtensions;
        void* m_deviceFeaturesChain{};
        bool m_withRayTracing{};
    };
}