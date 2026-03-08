// test_open.cpp — Tests for mocking the open() system call
//
// Demonstrates mocking POSIX open() via PLT/GOT patching.
// The open() function is a libc wrapper around the sys_open syscall.

#include <gmock-module.h>
#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;

// Declare the mock for open(const char*, int) -> int
// Note: open() has a 2-arg form (without mode) and 3-arg form (with mode).
// We mock the 2-arg form here. The variadic nature of open() means
// the PLT entry is for the same symbol regardless.
MOCK_MODULE_FUNC(int, open, const char*, int);

class OpenTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        VERIFY_AND_CLEAR_MODULE_FUNC(open);
    }
};

// Test 1: Mock open() to return a custom file descriptor
TEST_F(OpenTest, MockReturnsCustomFd)
{
    EXPECT_MODULE_FUNC_CALL(open, _, _)
        .WillOnce(Return(42));

    int fd = open("file.txt", O_RDONLY);
    EXPECT_EQ(fd, 42);
}

// Test 2: Mock open() to simulate an error (-1)
TEST_F(OpenTest, MockReturnsError)
{
    EXPECT_MODULE_FUNC_CALL(open, _, _)
        .WillOnce(Return(-1));

    int fd = open("nonexistent.txt", O_RDONLY);
    EXPECT_EQ(fd, -1);
}

// Test 3: Verify specific arguments are passed to open()
TEST_F(OpenTest, VerifyArguments)
{
    EXPECT_MODULE_FUNC_CALL(open,
        Eq(static_cast<const char*>("/tmp/test.txt")),
        Eq(O_RDONLY))
        .WillOnce(Return(10));

    int fd = open("/tmp/test.txt", O_RDONLY);
    EXPECT_EQ(fd, 10);
}

// Test 4: Multiple calls with different return values
TEST_F(OpenTest, MultipleCalls)
{
    EXPECT_MODULE_FUNC_CALL(open, _, _)
        .WillOnce(Return(3))
        .WillOnce(Return(4))
        .WillOnce(Return(-1));

    EXPECT_EQ(open("file1.txt", O_RDONLY), 3);
    EXPECT_EQ(open("file2.txt", O_WRONLY), 4);
    EXPECT_EQ(open("file3.txt", O_RDWR), -1);
}

// Test 5: Restore real open() and verify it works
TEST_F(OpenTest, RestoreRealFunction)
{
    // First, mock it
    EXPECT_MODULE_FUNC_CALL(open, _, _)
        .WillOnce(Return(999));

    int fd = open("dummy.txt", O_RDONLY);
    EXPECT_EQ(fd, 999);

    // Clear expectations and restore real function
    VERIFY_AND_CLEAR_MODULE_FUNC(open);
    RESTORE_MODULE_FUNC(open);

    // Now open() should call the real function
    // Try to open /dev/null — should succeed with a real fd
    fd = open("/dev/null", O_RDONLY);
    EXPECT_GE(fd, 0);  // real fd should be >= 0

    if (fd >= 0)
        close(fd);
}

// Test 6: Use INVOKE_REAL_MODULE_FUNC to delegate to real function
TEST_F(OpenTest, DelegateToReal)
{
    ON_MODULE_FUNC_CALL(open, _, _)
        .WillByDefault(Invoke([](const char* path, int flags) -> int {
            // Only mock for specific files, delegate rest to real
            if (std::string(path) == "virtual_file.txt")
                return 100;
            return INVOKE_REAL_MODULE_FUNC(open, path, flags);
        }));

    // This should return mocked value
    EXPECT_EQ(open("virtual_file.txt", O_RDONLY), 100);

    // This should call real open via INVOKE_REAL_MODULE_FUNC
    int fd = open("/dev/null", O_RDONLY);
    EXPECT_GE(fd, 0);
    if (fd >= 0)
        close(fd);
}
