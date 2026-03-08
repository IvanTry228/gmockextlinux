// test_read_write.cpp — Tests for mocking read() and write() system calls

#include <gmock-module.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::DoAll;
using ::testing::SetArrayArgument;

// Declare mocks for read and write
MOCK_MODULE_FUNC(ssize_t, read, int, void*, size_t);
MOCK_MODULE_FUNC(ssize_t, write, int, const void*, size_t);

class ReadWriteTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        VERIFY_AND_CLEAR_MODULE_FUNC(read);
        VERIFY_AND_CLEAR_MODULE_FUNC(write);
    }
};

// Test 1: Mock read() to return fake data
TEST_F(ReadWriteTest, MockReadReturnsFakeData)
{
    const char fake_data[] = "Hello from mock!";
    const ssize_t fake_len = static_cast<ssize_t>(sizeof(fake_data));

    ON_MODULE_FUNC_CALL(read, _, _, _)
        .WillByDefault(Invoke([&](int /*fd*/, void* buf, size_t count) -> ssize_t {
            size_t to_copy = std::min(count, sizeof(fake_data));
            std::memcpy(buf, fake_data, to_copy);
            return static_cast<ssize_t>(to_copy);
        }));

    char buffer[64] = {};
    ssize_t n = read(3, buffer, sizeof(buffer));

    EXPECT_EQ(n, fake_len);
    EXPECT_STREQ(buffer, "Hello from mock!");
}

// Test 2: Mock read() to simulate EOF
TEST_F(ReadWriteTest, MockReadReturnsEOF)
{
    EXPECT_MODULE_FUNC_CALL(read, _, _, _)
        .WillOnce(Return(0));

    char buffer[64] = {};
    ssize_t n = read(3, buffer, sizeof(buffer));
    EXPECT_EQ(n, 0);
}

// Test 3: Mock read() to simulate error
TEST_F(ReadWriteTest, MockReadReturnsError)
{
    EXPECT_MODULE_FUNC_CALL(read, _, _, _)
        .WillOnce(Return(-1));

    char buffer[64] = {};
    ssize_t n = read(999, buffer, sizeof(buffer));
    EXPECT_EQ(n, -1);
}

// Test 4: Mock write() to capture written data
TEST_F(ReadWriteTest, MockWriteCapturesData)
{
    std::string captured;

    ON_MODULE_FUNC_CALL(write, _, _, _)
        .WillByDefault(Invoke([&](int /*fd*/, const void* buf, size_t count) -> ssize_t {
            captured.assign(static_cast<const char*>(buf), count);
            return static_cast<ssize_t>(count);
        }));

    const char msg[] = "test output";
    ssize_t n = write(1, msg, sizeof(msg) - 1);

    EXPECT_EQ(n, static_cast<ssize_t>(sizeof(msg) - 1));
    EXPECT_EQ(captured, "test output");
}

// Test 5: Mock write() to simulate partial write
TEST_F(ReadWriteTest, MockWritePartial)
{
    EXPECT_MODULE_FUNC_CALL(write, _, _, _)
        .WillOnce(Return(5));  // Only 5 bytes written

    const char msg[] = "long message data";
    ssize_t n = write(1, msg, sizeof(msg) - 1);

    EXPECT_EQ(n, 5);
}

// Test 6: Verify specific fd is passed to read()
TEST_F(ReadWriteTest, VerifyFileDescriptor)
{
    EXPECT_MODULE_FUNC_CALL(read, Eq(42), _, _)
        .WillOnce(Return(0));

    char buffer[16] = {};
    ssize_t n = read(42, buffer, sizeof(buffer));
    EXPECT_EQ(n, 0);
}
