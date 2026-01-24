#pragma once

#include <oblo/core/flags.hpp>
#include <oblo/core/string/cstring_view.hpp>

namespace oblo::platform
{
    /// @brief Manages the loading and usage of shared (dynamic) libraries at runtime.
    ///
    /// Provides a platform-independent interface for opening, closing, and retrieving
    /// symbols from shared libraries. The class uses RAII to manage the library handle,
    /// unless explicitly leaked.
    class shared_library
    {
    public:
        enum class open_flags : u8
        {
            /// Prevents from adding prefixes or extension to the filename when looking for the library.
            exact_name,
            enum_max,
        };

    public:
        /// @brief Default constructor. Creates an empty (i.e. not open) shared library object.
        shared_library() = default;

        shared_library(const shared_library&) = delete;

        shared_library(shared_library&&) noexcept;

        /// @brief Constructs and opens a shared library from the specified path.
        /// @param path Path to the shared library file.
        /// @param flags Optional flags to control the open.
        explicit shared_library(cstring_view path, flags<open_flags> flags = {}) noexcept;

        shared_library& operator=(const shared_library&) = delete;

        shared_library& operator=(shared_library&&) noexcept;

        ~shared_library();

        /// @brief Opens a shared library from the given path.
        /// @param path Path to the shared library file.
        /// @param flags Optional flags to control the open.
        /// @return True if the library was successfully opened, false otherwise.
        bool open(cstring_view path, flags<open_flags> flags = {});

        /// @brief Closes the shared library if it is currently open.
        void close();

        /// @brief Leaks the library handle, preventing it from being closed on destruction.
        /// Useful when transferring ownership of the handle outside the class.
        void leak();

        /// @brief Checks whether the shared library is currently open.
        /// @return True if the handle was successfully opened, false otherwise.
        bool is_open() const;

        /// @brief Implicit conversion to bool. Equivalent to is_open().
        /// @return True if the library is open, false otherwise.
        explicit operator bool() const;

        /// @brief Retrieves a symbol (function or variable) from the shared library.
        /// @param name The name of the symbol to retrieve.
        /// @return Pointer to the symbol, or nullptr if not found.
        void* symbol(const char* name) const;

    private:
        void* m_handle{};
    };
}