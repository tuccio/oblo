#include <oblo/log/log.hpp>

extern "C"
{
    DOTNET_BINDINGS_API void oblo_log_debug(const char* message)
    {
        oblo::log::debug("{}", message);
    }

    DOTNET_BINDINGS_API void oblo_log_info(const char* message)
    {
        oblo::log::info("{}", message);
    }

    DOTNET_BINDINGS_API void oblo_log_warn(const char* message)
    {
        oblo::log::warn("{}", message);
    }

    DOTNET_BINDINGS_API void oblo_log_error(const char* message)
    {
        oblo::log::error("{}", message);
    }
}