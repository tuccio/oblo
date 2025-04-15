#include <oblo/dotnet/dotnet_runtime.hpp>
#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class dotnet_module final : public module_interface
    {
    public:
        DOTNET_RT_API bool startup(const module_initializer& initializer) override;

        DOTNET_RT_API bool finalize() override;

        DOTNET_RT_API void shutdown() override;

        DOTNET_RT_API const dotnet_runtime& get_runtime() const;

    private:
        dotnet_runtime m_runtime;
    };
}