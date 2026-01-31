#include <oblo/modules/module_interface.hpp>

namespace oblo
{
    class dotnet_behaviour_module final : public module_interface
    {
    public:
        OBLO_DOTNET_BEHAVIOUR_API bool startup(const module_initializer& initializer) override;

        OBLO_DOTNET_BEHAVIOUR_API bool finalize() override;

        OBLO_DOTNET_BEHAVIOUR_API void shutdown() override;
    };
}