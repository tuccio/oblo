#include <oblo/dotnet/dotnet_runtime.hpp>

#include <oblo/core/platform/shared_library.hpp>
#include <oblo/dotnet/sdk/coreclr_delegates.h>
#include <oblo/dotnet/sdk/hostfxr.h>

#include <iostream>

namespace oblo
{
    namespace
    {

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
            [[maybe_unused]] constexpr const char_t* configPath =
                L"./dotnet/shared/Microsoft.NETCore.App/9.0.4/dotnet.runtimeconfig.json";

            // Load .NET Core
            void* load_assembly_and_get_function_pointer = nullptr;
            hostfxr_handle cxt = nullptr;
            int rc = hostfxrInit(configPath, nullptr, &cxt);

            if (rc != 0 || cxt == nullptr)
            {
                std::cerr << "Init failed: " << std::hex << std::showbase << rc << std::endl;
                hostfxrClose(cxt);
                return false;
            }

            // Get the load assembly function pointer
            rc = hostfxrGetDelegate(cxt,
                hdt_load_assembly_and_get_function_pointer,
                &load_assembly_and_get_function_pointer);

            if (rc != 0 || load_assembly_and_get_function_pointer == nullptr)
            {
                std::cerr << "Get delegate failed: " << std::hex << std::showbase << rc << std::endl;
            }

            hostfxrClose(cxt);

            dotnetLoadAssembly = load_assembly_and_get_function_pointer_fn(load_assembly_and_get_function_pointer);
            return true;
        }

        template <typename T>
            requires std::is_function_v<T>
        std::add_pointer_t<T> load_assembly_delegate(
            const char_t* assemblyPath, const char_t* assemblyQualifiedType, const char_t* method)
        {
            std::add_pointer_t<T> delegate{};

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

        const auto delegate = r->load_assembly_delegate<void()>(
            L".\\..\\..\\..\\..\\oblo\\dotnet\\managed\\Oblo.Runtime\\bin\\Debug\\net9.0\\Oblo.Runtime.dll",
            L"Oblo.Runtime.RuntimeHost, Oblo.Runtime",
            L"Init");

        if (delegate)
        {
            delegate();
        }

        m_impl = std::move(r);

        return no_error;
    }

    void dotnet_runtime::shutdown()
    {
        m_impl.reset();
    }
}