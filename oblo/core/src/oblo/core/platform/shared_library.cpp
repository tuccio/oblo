#include <oblo/core/platform/shared_library.hpp>

#if defined(WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace oblo::platform
{
    shared_library::shared_library(shared_library&& other) noexcept : m_handle{other.m_handle}
    {
        other.m_handle = nullptr;
    }

    shared_library::shared_library(const std::filesystem::path& path)
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

    bool shared_library::open(const std::filesystem::path& path)
    {
        close();

        m_handle = LoadLibraryW(path.native().c_str());
        return m_handle != nullptr;
    }

    void shared_library::close()
    {
        if (m_handle)
        {
            FreeLibrary(HMODULE(m_handle));
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
        return reinterpret_cast<void*>(GetProcAddress(HMODULE(m_handle), name));
    }
}