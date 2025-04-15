#include <oblo/dotnet/dotnet_runtime.hpp>

#include <oblo/core/platform/shared_library.hpp>
#include <oblo/core/string/string_builder.hpp>

#include <oblo/log/log.hpp>

#include <utf8cpp/utf8.h>

#include <coreclr_delegates.h>
#include <hostfxr.h>

#include <string>

namespace oblo
{
    namespace
    {
        enum corehost_status_code
        {
            // Success
            Success = 0, // Operation was successful
            Success_HostAlreadyInitialized =
                0x00000001, // Initialization was successful, but another host context is already initialized
            Success_DifferentRuntimeProperties =
                0x00000002, // Initialization was successful, but another host context is already initialized and the
                            // requested context specified runtime properties which are not the same
        };
    }

    struct dotnet_runtime::impl
    {
        bool load_hostfxr()
        {
            constexpr const char* hostfxrPath = "./dotnet/host/fxr/9.0.4/hostfxr";

            hostfxr = platform::shared_library{hostfxrPath};

            if (!hostfxr)
            {
                return false;
            }

            hostfxrInit =
                hostfxr_initialize_for_runtime_config_fn(hostfxr.symbol("hostfxr_initialize_for_runtime_config"));
            hostfxrGetDelegate = hostfxr_get_runtime_delegate_fn(hostfxr.symbol("hostfxr_get_runtime_delegate"));
            hostfxrClose = hostfxr_close_fn(hostfxr.symbol("hostfxr_close"));

            return (hostfxrInit && hostfxrGetDelegate && hostfxrClose);
        }

        bool load_dotnet_load_assembly()
        {
            [[maybe_unused]] constexpr const char_t* configPath = L"./dotnet/oblo.runtimeconfig.json";

            // Load .NET Core
            void* load_assembly_and_get_function_pointer = nullptr;
            hostfxr_handle ctx = nullptr;
            int rc = hostfxrInit(configPath, nullptr, &ctx);

            const auto failedToInitialize = rc != corehost_status_code::Success &&
                rc != corehost_status_code::Success_DifferentRuntimeProperties &&
                rc != corehost_status_code::Success_HostAlreadyInitialized;

            if (failedToInitialize || ctx == nullptr)
            {
                oblo::log::error("Failed to initialize runtime: {:#x}", rc);
                hostfxrClose(ctx);
                return false;
            }

            // Get the load assembly function pointer
            rc = hostfxrGetDelegate(ctx,
                hdt_load_assembly_and_get_function_pointer,
                &load_assembly_and_get_function_pointer);

            if (rc != 0 || load_assembly_and_get_function_pointer == nullptr)
            {
                oblo::log::error("Failed to get delegate: {:#x}", rc);
            }

            hostfxrClose(ctx);

            dotnetLoadAssembly = load_assembly_and_get_function_pointer_fn(load_assembly_and_get_function_pointer);

            // Internally CoreCLR is not unloaded (e.g. hostpolicy.dll isn't)
            // As a result, unloading hostfxr will cause any next attempt to initialize a new runtime to fail,
            // causing a new instance of this module to fail to initialize, e.g. in smoke tests where we
            // wipe the module_manager at each run.
            // To avoid this, we avoid unloading hostfxr, in order to let it keep track of the previously loaded CLR.
            hostfxr.leak();

            return true;
        }

        void* load_assembly_delegate(
            const char_t* assemblyPath, const char_t* assemblyQualifiedType, const char_t* method)
        {
            void* delegate{};

            const int rc = dotnetLoadAssembly(assemblyPath,
                assemblyQualifiedType,
                method,
                UNMANAGEDCALLERSONLY_METHOD,
                nullptr,
                reinterpret_cast<void**>(&delegate));

            return rc == 0 ? delegate : nullptr;
        }

        platform::shared_library hostfxr;
        hostfxr_initialize_for_runtime_config_fn hostfxrInit{};
        hostfxr_get_runtime_delegate_fn hostfxrGetDelegate{};
        hostfxr_close_fn hostfxrClose{};
        load_assembly_and_get_function_pointer_fn dotnetLoadAssembly{};
    };

    dotnet_runtime::dotnet_runtime() = default;

    dotnet_runtime::~dotnet_runtime() = default;

    expected<> dotnet_runtime::init()
    {
        auto r = allocate_unique<impl>();

        if (!r->load_hostfxr())
        {
            return unspecified_error;
        }

        if (!r->load_dotnet_load_assembly())
        {
            return unspecified_error;
        }

        m_impl = std::move(r);

        return no_error;
    }

    void dotnet_runtime::shutdown()
    {
        m_impl.reset();
    }

#if _WIN32
    void* dotnet_runtime::load_assembly_delegate(
        cstring_view assemblyPath, cstring_view assemblyType, cstring_view methodName) const
    {
        std::wstring assemblyPathU16;
        utf8::unchecked::utf8to16(assemblyPath.begin(), assemblyPath.end(), std::back_inserter(assemblyPathU16));

        std::wstring assemblyTypeU16;
        utf8::unchecked::utf8to16(assemblyType.begin(), assemblyType.end(), std::back_inserter(assemblyTypeU16));

        std::wstring methodNameU16;
        utf8::unchecked::utf8to16(methodName.begin(), methodName.end(), std::back_inserter(methodNameU16));

        return m_impl->load_assembly_delegate(assemblyPathU16.c_str(), assemblyTypeU16.c_str(), methodNameU16.c_str());
    }
#else
    void* dotnet_runtime::load_assembly_delegate(
        cstring_view assemblyPath, cstring_view assemblyType, cstring_view methodName) const
    {
        return m_impl->load_assembly_delegate(assemblyPath.c_str(), assemblyType.c_str(), methodName.c_str());
    }
#endif
}