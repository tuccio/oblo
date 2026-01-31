#include <oblo/log/log.hpp>

extern "C"
{
    OBLO_DOTNET_BINDINGS_API void oblo_log_debug(const char* message)
    {
        oblo::log::debug("{}", message);
    }

    OBLO_DOTNET_BINDINGS_API void oblo_log_info(const char* message)
    {
        oblo::log::info("{}", message);
    }

    OBLO_DOTNET_BINDINGS_API void oblo_log_warn(const char* message)
    {
        oblo::log::warn("{}", message);
    }

    OBLO_DOTNET_BINDINGS_API void oblo_log_error(const char* message)
    {
        oblo::log::error("{}", message);
    }
}