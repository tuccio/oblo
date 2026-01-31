#include <oblo/dotnet/dotnet_runtime.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class dotnet_module final : public module_interface
    {
    public:
        OBLO_DOTNET_RUNTIME_API bool startup(const module_initializer& initializer) override;

        OBLO_DOTNET_RUNTIME_API bool finalize() override;

        OBLO_DOTNET_RUNTIME_API void shutdown() override;

        OBLO_DOTNET_RUNTIME_API const dotnet_runtime& get_runtime() const;

    private:
        dotnet_runtime m_runtime;
    };
}