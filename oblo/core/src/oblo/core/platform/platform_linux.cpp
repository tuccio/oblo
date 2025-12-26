#ifdef __linux__

    #include <oblo/core/buffered_array.hpp>
    #include <oblo/core/debug.hpp>
    #include <oblo/core/filesystem/file.hpp>
    #include <oblo/core/filesystem/filesystem.hpp>
    #include <oblo/core/platform/core.hpp>
    #include <oblo/core/platform/file.hpp>
    #include <oblo/core/platform/process.hpp>
    #include <oblo/core/platform/shell.hpp>
    #include <oblo/core/uuid.hpp>
    #include <oblo/core/uuid_generator.hpp>

    #include <cerrno>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <uuid/uuid.h>

    #include <bit>
    #include <charconv>
    #include <cstdio>

namespace oblo::platform
{
    bool init()
    {
        return true;
    }

    void shutdown() {}

    void debug_output(const char* str)
    {
        std::fputs(str, stderr);
    }

    bool is_debugger_attached()
    {
        buffered_array<char, 4096> buf;

        const auto r = filesystem::load_text_file_into_memory(buf, "/proc/self/status");

        if (!r)
        {
            return false;
        }

        const string_view str{r->data(), r->data() + r->size()};

        constexpr string_view tracerPidStr = "TracerPid:";

        const auto tracerIdx = str.find_first_of(tracerPidStr);

        if (tracerIdx == string_view::npos)
        {
            return false;
        }

        const char* tracer = str.data() + tracerIdx + tracerPidStr.size();

        // Skip whitespace
        while (std::isspace(*tracer))
        {
            ++tracer;
        }

        const char* end = tracer;

        while (*end >= '0' && *end <= '9')
        {
            ++end;
        }

        if (tracer == end)
        {
            return false;
        }

        // Finally try to read the PID
        int tracerPid = 0;
        const auto [ptr, ec] = std::from_chars(tracer, end, tracerPid);

        if (ec != std::errc{})
        {
            return false;
        }

        return tracerPid != 0;

        return false;
    }

    void wait_for_attached_debugger()
    {
        while (!is_debugger_attached())
        {
        }
    }

    void open_file(string_view dir)
    {
        // TODO
        OBLO_ASSERT(false, "Not implemented yet");
    }

    void open_folder(string_view dir)
    {
        // TODO
        OBLO_ASSERT(false, "Not implemented yet");
    }

    bool open_file_dialog(string_builder& file)
    {
        // TODO
        OBLO_ASSERT(false, "Not implemented yet");
        return false;
    }

    bool search_program_files(string_builder& out, string_view relativePath)
    {
        // TODO
        OBLO_ASSERT(false, "Not implemented yet");
        return false;
    }

    bool read_environment_variable(string_builder& out, cstring_view key)
    {
        const char* const result = getenv(key.c_str());

        if (result)
        {
            out.append(result);
        }

        return result != nullptr;
    }

    bool read_environment_variable(cstring_view key, cstring_view value)
    {
        return setenv(key.c_str(), value.c_str(), 1) == 0;
    }

    process::process() = default;

    process::process(process&& other) noexcept : m_pid(other.m_pid)
    {
        other.m_pid = -1;
    }

    process::~process()
    {
        detach();
    }

    process& process::operator=(process&& other) noexcept
    {
        if (this != &other)
        {
            detach();
            m_pid = other.m_pid;
            other.m_pid = -1;
        }
        return *this;
    }

    expected<> process::start(const process_descriptor& desc)
    {
        detach();

        const pid_t pid = fork();
        if (pid < 0)
        {
            return unspecified_error;
        }

        if (pid == 0)
        {
            // Child branch of the fork

            if (desc.inputStream)
            {
                dup2(desc.inputStream->get_native_handle(), STDIN_FILENO);
            }

            if (desc.outputStream)
            {
                dup2(desc.outputStream->get_native_handle(), STDOUT_FILENO);
            }

            if (desc.errorStream)
            {
                dup2(desc.errorStream->get_native_handle(), STDERR_FILENO);
            }

            if (!desc.workDir.empty())
            {
                chdir(desc.workDir.c_str());
            }

            // Build argv
            const size_t argc = desc.arguments.size() + 2;
            char** argv = static_cast<char**>(alloca(sizeof(char*) * argc));

            argv[0] = const_cast<char*>(desc.path.c_str());

            for (size_t i = 0; i < desc.arguments.size(); ++i)
            {
                argv[i + 1] = const_cast<char*>(desc.arguments[i].c_str());
            }

            argv[argc - 1] = nullptr;

            execvp(desc.path.c_str(), argv);

            // exec failed
            _exit(127);
        }

        // Parent from here on, the child won't return here
        m_pid = pid;
        return no_error;
    }

    bool process::is_done()
    {
        if (m_pid < 0)
        {
            return true;
        }

        int status = 0;
        const pid_t result = waitpid(m_pid, &status, WNOHANG);

        return result == m_pid;
    }

    expected<> process::wait()
    {
        if (m_pid < 0)
        {
            return no_error;
        }

        int status = 0;
        if (waitpid(m_pid, &status, 0) < 0)
        {
            return unspecified_error;
        }

        return no_error;
    }

    expected<i64> process::get_exit_code()
    {
        if (m_pid < 0)
        {
            return unspecified_error;
        }

        int status = 0;
        const pid_t result = waitpid(m_pid, &status, WNOHANG);

        if (result == 0)
        {
            return unspecified_error; // still running
        }

        if (result < 0)
        {
            return unspecified_error;
        }

        if (WIFEXITED(status))
        {
            return i64{WEXITSTATUS(status)};
        }

        if (WIFSIGNALED(status))
        {
            return i64{128 + WTERMSIG(status)};
        }

        return unspecified_error;
    }

    void process::detach()
    {
        // On Linux, processes are not "closed" like handles.
        // Detach simply forgets the pid.
        m_pid = -1;
    }

    namespace
    {
        file::error translate_file_error()
        {
            using error = file::error;

            switch (errno)
            {
            case EPIPE:
                return error::eof;

            default:
                return error::unspecified;
            }
        }
    }

    expected<> file::create_pipe(file& readPipe, file& writePipe, u32 /*bufferSizeHint*/)
    {
        readPipe.close();
        writePipe.close();

        int fds[2]{};

        if (pipe2(fds, O_CLOEXEC) != 0)
        {
            return unspecified_error;
        }

        readPipe.m_handle = fds[0];
        writePipe.m_handle = fds[1];

        return no_error;
    }

    file::file() noexcept = default;

    file::file(file&& other) noexcept : m_handle(other.m_handle)
    {
        other.m_handle = -1;
    }

    file::~file()
    {
        close();
    }

    file& file::operator=(file&& other) noexcept
    {
        if (this != &other)
        {
            close();
            m_handle = other.m_handle;
            other.m_handle = -1;
        }
        return *this;
    }

    expected<u32, file::error> file::read(void* dst, u32 size) const noexcept
    {
        const ssize_t result = ::read(m_handle, dst, size);

        if (result < 0)
        {
            return translate_file_error();
        }

        if (result == 0)
        {
            return file::error::eof;
        }

        return static_cast<u32>(result);
    }

    expected<u32, file::error> file::write(const void* src, u32 size) const noexcept
    {
        const ssize_t result = ::write(m_handle, src, size);

        if (result < 0)
        {
            return translate_file_error();
        }

        return static_cast<u32>(result);
    }

    bool file::is_open() const noexcept
    {
        return m_handle >= 0;
    }

    void file::close() noexcept
    {
        if (m_handle < 0)
        {
            return;
        }

        [[maybe_unused]] const int result = ::close(m_handle);
        OBLO_ASSERT(result == 0);

        m_handle = -1;
    }

    file::operator bool() const noexcept
    {
        return m_handle >= 0;
    }

    file::native_handle file::get_native_handle() const noexcept
    {
        return m_handle;
    }
}

namespace oblo
{
    uuid uuid_system_generator::generate() const
    {
        uuid_t u;
        uuid_generate(u);
        return std::bit_cast<uuid>(u);
    }
}

#endif