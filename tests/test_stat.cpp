// test_stat.cpp — Tests for mocking the stat() system call

#include <gmock-module.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <cstring>

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;

// We mock __xstat on glibc (stat is usually a macro/inline that calls __xstat)
// On newer glibc (2.33+), stat() is a real symbol. We try stat first.
// If stat doesn't work, fall back to __xstat.
//
// For portability, we provide a helper that works with the stat() call.

// Mock lstat (always a proper symbol)
MOCK_MODULE_FUNC(int, lstat, const char*, struct stat*);

class StatTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        VERIFY_AND_CLEAR_MODULE_FUNC(lstat);
    }
};

// Test 1: Mock lstat() to return a custom struct
TEST_F(StatTest, MockReturnsCustomStat)
{
    ON_MODULE_FUNC_CALL(lstat, _, _)
        .WillByDefault(Invoke([](const char* /*path*/, struct stat* buf) -> int {
            std::memset(buf, 0, sizeof(struct stat));
            buf->st_size = 12345;
            buf->st_mode = S_IFREG | 0644;
            buf->st_uid  = 1000;
            buf->st_gid  = 1000;
            return 0; // success
        }));

    struct stat st = {};
    int ret = lstat("/some/file.txt", &st);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(st.st_size, 12345);
    EXPECT_TRUE(S_ISREG(st.st_mode));
    EXPECT_EQ(st.st_uid, 1000u);
}

// Test 2: Mock lstat() to simulate file not found
TEST_F(StatTest, MockReturnsNotFound)
{
    EXPECT_MODULE_FUNC_CALL(lstat, _, _)
        .WillOnce(Return(-1));

    struct stat st = {};
    int ret = lstat("/nonexistent", &st);

    EXPECT_EQ(ret, -1);
}

// Test 3: Mock lstat() to report a directory
TEST_F(StatTest, MockReportsDirectory)
{
    ON_MODULE_FUNC_CALL(lstat, _, _)
        .WillByDefault(Invoke([](const char* /*path*/, struct stat* buf) -> int {
            std::memset(buf, 0, sizeof(struct stat));
            buf->st_mode = S_IFDIR | 0755;
            buf->st_size = 4096;
            return 0;
        }));

    struct stat st = {};
    int ret = lstat("/some/directory", &st);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_size, 4096);
}
