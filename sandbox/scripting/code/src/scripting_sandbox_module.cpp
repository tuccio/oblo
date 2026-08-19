#include <oblo/log/log.hpp>
#include <oblo/modules/module_interface.hpp>
#include <oblo/modules/utility/registration.hpp>
#include <oblo/reflection/codegen/registration.hpp>

#include <scripting_sandbox_interface.hpp>

#include <source_location>

namespace oblo::sandbox
{
    class scripting_sandbox_module final : public module_interface
    {
    public:
        bool startup(const module_initializer&) override
        {
            reflection::gen::load_module_and_register();
            return true;
        }

        bool finalize() override
        {
            return true;
        }

        void shutdown() override {}
    };

    void log_from_native()
    {
        constexpr auto loc = std::source_location::current();

        log::info("Log from {}", loc.function_name());
    }

    f32 compute_scaled(i32 count, f32 scale, vec3 offset, bool enabled)
    {
        if (!enabled)
        {
            return 0.f;
        }

        return f32(count) * scale + offset.x + offset.y + offset.z;
    }
}

OBLO_MODULE_REGISTER(oblo::sandbox::scripting_sandbox_module)