#include <oblo/core/platform/shared_library.hpp>

#if defined(WIN32)
    #define NOMINMAX
    #include <Windows.h>

    #include <oblo/core/platform/platform_win32.hpp>
#endif

namespace oblo::platform
{
    shared_library::shared_library(shared_library&& other) noexcept : m_handle{other.m_handle}
    {
        other.m_handle = nullptr;
    }

    shared_library::shared_library(cstring_view path)
    {
        open(path);
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

    bool shared_library::open(cstring_view path)
    {
        close();

#ifdef _WIN32
        wchar_t buf[win32::MaxPath];
        win32::convert_path(path, buf);

        m_handle = LoadLibraryW(buf);
        return m_handle != nullptr;
#else
    #error "Not implemented"
#endif
    }

    void shared_library::close()
    {
        if (m_handle)
        {
#ifdef _WIN32
            FreeLibrary(HMODULE(m_handle));
#else
    #error "Not implemented"
#endif
            m_handle = nullptr;
        }
    }

    bool shared_library::is_valid() const
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
    #error "Not implemented"
#endif
    }
}