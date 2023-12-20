#include <gtest/gtest.h>
#include <ulib/process.h>

TEST(Process, EchoArgs)
{
    ulib::process proc(u8"echo", {u8"test_text"}, ulib::process::pipe_stdout);
    int code = proc.wait();

    ulib::string out = (proc.out().read_all());
    if (out.ends_with('\n'))
        out.pop_back();

    ASSERT_EQ(out, "test_text");
}

TEST(Process, EchoLine)
{
    ulib::process proc(u8"echo test_text", ulib::process::pipe_stdout);
    int code = proc.wait();

    ulib::string out = (proc.out().read_all());
    if (out.ends_with('\n'))
        out.pop_back();

    ASSERT_EQ(out, "test_text");
}

TEST(Process, FileNotFoundError)
{
    ASSERT_THROW({ ulib::process proc(u8"ech111221ddo test_text"); }, ulib::process_file_not_found_error);
}

TEST(Process, InvalidWorkingDirectoryError)
{
    ASSERT_THROW({ ulib::process proc(u8"echo test_text", ulib::process::noflags, "shfjhsaflkasjfa1123d"); }, ulib::process_invalid_working_directory_error);
}