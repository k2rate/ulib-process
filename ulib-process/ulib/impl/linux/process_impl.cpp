#include "../archdef.h"

#ifdef ULIB_PROCESS_LINUX
#include "process.h"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ulib/format.h>

#include "../../process_exceptions.h"

extern char **environ;

namespace ulib
{

    process::bpipe::~bpipe() { close(); }

    process::bpipe &process::bpipe::operator=(bpipe &&other)
    {
        close();

        mHandle = other.mHandle;
        other.mHandle = 0;

        return *this;
    }

    void process::bpipe::close()
    {
        if (mHandle)
        {
            ::close(mHandle);
            mHandle = 0;
        }
    }

    size_t process::rpipe::read(void *buf, size_t size) { return ::read(mHandle, buf, size); }
    ulib::string process::rpipe::read_all()
    {
        ulib::string result;
        char reading_buf[1];
        while (::read(mHandle, reading_buf, 1) > 0)
        {
            result.append(ulib::string_view{reading_buf, 1});
        }

        return result;
    }

    char process::rpipe::getchar()
    {
        char ch;
        this->read(&ch, 1);
        return ch;
    }

    ulib::string process::rpipe::getline()
    {
        ulib::string str;
        while (true)
        {
            char ch = getchar();
            if (ch == '\n')
                break;

            str.push_back(ch);
        }

        return str;
    }

    size_t process::wpipe::write(const void *buf, size_t size) { return ::write(mHandle, buf, size); }
    size_t process::wpipe::write(ulib::string_view str) { return ::write(mHandle, str.data(), str.size()); }

    process::process() { mHandle = 0; }
    process::process(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags,
                     std::optional<std::filesystem::path> workingDirectory)
    {
        this->run(path, args, flags, workingDirectory);
    }
    process::process(ulib::u8string_view line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        this->run(line, flags, workingDirectory);
    }

    process::process(process &&other)
    {
        mHandle = other.mHandle;
        other.mHandle = 0;

        mInPipe = std::move(other.mInPipe);
        mOutPipe = std::move(other.mOutPipe);
        mErrPipe = std::move(other.mErrPipe);

        if (mHandle)
        {
            // if (is_running())
            // {

            // }
        }
    }

    process::~process()
    {
        if (mHandle)
        {
            // if (is_running())
            // {

            // }
        }
    }

    process &process::operator=(process &&other) {}

    void MakeExecveArgs(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args) {}

    ulib::list<ulib::u8string> cmdline_to_args(ulib::u8string_view line)
    {
        bool inQuotes = false;
        ulib::u8string curr = line;
        for (auto &ch : curr)
        {
            if (inQuotes)
            {
                if (ch == '\"')
                {
                    inQuotes = false;
                    ch = 0;
                }
            }
            else
            {
                if (ch == '\"')
                {
                    inQuotes = true;
                    ch = 0;
                }
                else if (ch == ' ')
                {
                    ch = '\0';
                }
            }
        }

        using ChT = typename ulib::u8string_view::value_type;

        ulib::list<ulib::u8string> result;
        const char *delim = "\0";
        auto spl = curr.split(ulib::u8string_view{(ChT *)delim, (ChT *)delim + 1});
        for (auto word : spl)
        {
            result.push_back(word);
        }

        return result;
    }

    ulib::u8string u8path_to_artifact_name(ulib::u8string_view path)
    {
        ulib::u8string firstArg;
        ulib::u8string callStr{path};
        size_t pos = callStr.rfind('/');
        if (pos == ulib::npos)
        {
            firstArg = callStr;
        }
        else
        {
            size_t idx = pos + 1;
            // printf("idx: %d, diff: %d\n", (int)idx, (int)(callStr.size() - idx));

            firstArg = callStr.substr(idx, callStr.size() - idx);
        }

        return firstArg;
    }

    void process::run(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags,
                      std::optional<std::filesystem::path> workingDirectory)
    {
        ulib::u8string callStr{path.u8string()};
        ulib::u8string firstArg = u8path_to_artifact_name(callStr);
        firstArg.MarkZeroEnd();

        ulib::list<ulib::u8string> zargs = args;
        for (auto &zarg : zargs)
            zarg.MarkZeroEnd();

        ulib::list<const char *> argvList;
        argvList.push_back((char *)firstArg.data());
        for (auto &arg : zargs)
            argvList.push_back((char *)arg.data());
        argvList.push_back(NULL);

        if (workingDirectory)
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(),
                      (const char *)workingDirectory->u8string().c_str(), flags);
        }
        else
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(), nullptr, flags);
        }
    }

    void process::run(ulib::u8string_view line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        auto args = cmdline_to_args(line);
        if (args.size() == 0)
            throw process_internal_error{"invalid command line"};

        ulib::u8string callStr = args.front();
        ulib::u8string firstArg = u8path_to_artifact_name(callStr);
        args.front() = firstArg;

        for (auto &arg : args)
            arg.MarkZeroEnd();

        callStr.MarkZeroEnd();

        ulib::list<const char *> argvList;
        // argvList.push_back((char *)firstArg.data());
        for (auto &arg : args)
            argvList.push_back((char *)arg.data());
        argvList.push_back(NULL);

        if (workingDirectory)
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(),
                      (const char *)workingDirectory->u8string().c_str(), flags);
        }
        else
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(), nullptr, flags);
        }
    }

    void process::run(const char *path, char **argv, const char *workingDirectory, uint32 flags)
    {
        struct errdata
        {
            int type, code;
        };

        int fd_sink[2];
        int fd_stdin[2];
        int fd_stdout[2];
        int fd_stderr[2];

        if (flags & pipe_stdin)
        {
            pipe(fd_stdin);
        }

        if (flags & pipe_stdout)
        {
            pipe(fd_stdout);
        }

        if (flags & pipe_stderr)
        {
            pipe(fd_stderr);
        }

        pipe(fd_sink);

        if (::fcntl(fd_sink[1], F_SETFD, FD_CLOEXEC) == -1)
        {
            // error ...
        }

        auto close_all_pipes = [&]() {
            if (flags & pipe_stdin)
            {
                close(fd_stdin[0]);
                close(fd_stdin[1]);
            }

            if (flags & pipe_stdout)
            {
                close(fd_stdout[0]);
                close(fd_stdout[1]);
            }

            if (flags & pipe_stderr)
            {
                close(fd_stderr[0]);
                close(fd_stderr[1]);
            }

            close(fd_sink[0]);
            close(fd_sink[1]);
        };

        int pid = fork();
        if (pid == 0)
        {
            if (flags & pipe_stdin)
            {
                int fn = fileno(stdin);
                close(fn);
                dup2(fd_stdin[0], fn);
            }

            if (flags & pipe_stdout)
            {
                int fn = fileno(stdout);
                close(fn);
                dup2(fd_stdout[1], fn);
            }

            if (flags & pipe_stderr)
            {
                int fn = fileno(stderr);
                close(fn);
                dup2(fd_stderr[1], fn);
            }

            close(fd_sink[0]);

            if (workingDirectory)
            {
                if (chdir(workingDirectory) == -1)
                {
                    errdata ed {1, errno};
                    ::write(fd_sink[1], &ed, sizeof(errdata));
                    ::close(fd_sink[1]);
                    ::_exit(EXIT_FAILURE);
                }
            }

            // child
            // char *argv[] = {(char *)firstArg.c_str(), NULL};
            // char *envp[] = {(char *)"some", NULL};

            execvp(path, argv);
            // execve(path, argv, environ);

            {
                errdata ed {0, errno};
                ::write(fd_sink[1], &ed, sizeof(errdata));
                ::close(fd_sink[1]);
                ::_exit(EXIT_FAILURE);
            }

            // perror(path);
        }
        else if (pid == -1)
        {
            close_all_pipes();

            // fork error
            throw process_internal_error{"fork failed"};
        }
        else
        {
            {
                close(fd_sink[1]);

                errdata ed;
                int rv = ::read(fd_sink[0], &ed, sizeof(errdata));
                if (rv == 0)
                {
                    // no error
                }
                else if (rv == sizeof(errdata))
                {
                    close_all_pipes();

                    if (ed.type == 0 && ed.code == ENOENT)
                    {
                        // execv
                        throw process_file_not_found_error{std::strerror(ed.code)};
                    }
                    else if (ed.type == 1 && ed.code == ENOENT)
                    {
                        // chdir
                        throw process_invalid_working_directory_error{std::strerror(ed.code)};
                    }
                    else
                    {
                        throw process_internal_error{ulib::format("({}) errno [{}]: {}", ed.type, ed.code, std::strerror(ed.code))};
                    }
                }
                else if (rv == -1)
                {
                    close_all_pipes();
                    throw process_internal_error{"read sink pipe failed"};
                }
            }

            if (flags & pipe_stdin)
            {
                mInPipe = std::move(wpipe{fd_stdin[1]});
                close(fd_stdin[0]);
            }

            if (flags & pipe_stdout)
            {
                mOutPipe = std::move(rpipe{fd_stdout[0]});
                close(fd_stdout[1]);
            }

            if (flags & pipe_stderr)
            {
                mErrPipe = std::move(rpipe{fd_stdout[0]});
                close(fd_stdout[1]);
            }

            mHandle = pid;
        }
    }

    std::optional<int> process::wait(std::chrono::milliseconds ms) {}

    int process::wait()
    {
        int wstatus;
        int result = waitpid(mHandle, &wstatus, 0);
        if (result == -1)
        {
            throw ulib::RuntimeError{"waitpid failed"};
        }

        return WEXITSTATUS(wstatus);
    }

    bool process::is_running()
    {
        int wstatus;
        int result = waitpid(mHandle, &wstatus, WNOHANG);
        if (result == -1)
        {
            throw ulib::RuntimeError{"waitpid failed"};
        }
        else if (result == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    bool process::is_finished() { return !is_running(); }
    void process::detach() {} // already detached

    std::optional<int> process::check()
    {
        int wstatus;
        int result = waitpid(mHandle, &wstatus, WNOHANG);
        if (result == -1)
        {
            throw ulib::RuntimeError{"waitpid failed"};
        }
        else if (result == 0)
        {
            return std::nullopt;
        }
        else
        {
            return WEXITSTATUS(wstatus);
        }
    }

} // namespace ulib

#endif