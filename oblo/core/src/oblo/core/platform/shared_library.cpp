#include <oblo/core/platform/shared_library.hpp>

#if defined(WIN32)
    #define NOMINMAX
    #include <Windows.h>

    #include <oblo/core/platform/platform_win32.hpp>
#elif defined(__linux__)
    #include <dlfcn.h>
#endif

namespace oblo::platform
{
    shared_library::shared_library(shared_library&& other) noexcept : m_handle{other.m_handle}
    {
        other.m_handle = nullptr;
    }

    shared_library::shared_library(cstring_view path, flags<open_flags> flags)
    {
        open(path, flags);
    }

    shared_library& shared_library::operator=(shared_library&& other) noexcept
    {
        close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

    shared_library::~shared_library()
    {
        close();
    }

    bool shared_library::open(cstring_view path, flags<open_flags> flags)
    {
        close();

#ifdef _WIN32
        wchar_t buf[win32::MaxPath];
        wchar_t* const end = win32::convert_path(path, buf);

        if (flags.contains(open_flags::exact_name))
        {
            // LoadLibrary will append .dll to filenames without extension
            // In order to avoid that, we need to add a trailing '.' to the path
            if (end < buf + (win32::MaxPath - 1))
            {
                *end = '.';
                *(end + 1) = '\0';
            }
        }

        m_handle = LoadLibraryW(buf);

#else
        m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        return m_handle != nullptr;
    }

    void shared_library::close()
    {
        if (m_handle)
        {
#ifdef _WIN32
            FreeLibrary(HMODULE(m_handle));
#else
            dlclose(m_handle);
#endif
            m_handle = nullptr;
        }
    }

    void shared_library::leak()
    {
        m_handle = nullptr;
    }

    bool shared_library::is_open() const
    {
        return m_handle != nullptr;
    }

    shared_library::operator bool() const
    {
        return m_handle != nullptr;
    }

    void* shared_library::symbol(const char* name) const
    {
#ifdef _WIN32
        return reinterpret_cast<void*>(GetProcAddress(HMODULE(m_handle), name));
#else
        return dlsym(m_handle, name);
#endif
    }
}