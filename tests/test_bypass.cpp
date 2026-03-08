// test_bypass.cpp — Tests for the BYPASS_MOCKS mechanism

#include <gmock-module.h>
#include <gtest/gtest.h>
#include <unistd.h>

using ::testing::_;
using ::testing::Return;

// Mock getpid() — a simple 0-arg function
MOCK_MODULE_FUNC(pid_t, getpid);

class BypassTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        VERIFY_AND_CLEAR_MODULE_FUNC(getpid);
    }
};

// Test 1: Basic mock intercepts getpid()
TEST_F(BypassTest, MockInterceptsGetpid)
{
    EXPECT_MODULE_FUNC_CALL(getpid)
        .WillOnce(Return(42));

    pid_t pid = getpid();
    EXPECT_EQ(pid, 42);
}

// Test 2: BYPASS_MOCKS passes through to real function
TEST_F(BypassTest, BypassPassesThrough)
{
    ON_MODULE_FUNC_CALL(getpid)
        .WillByDefault(Return(42));

    // With mock active, should return 42
    pid_t mocked_pid = getpid();
    EXPECT_EQ(mocked_pid, 42);

    // With bypass, should return real pid (which is > 0)
    pid_t real_pid = 0;
    BYPASS_MOCKS(real_pid = getpid());
    EXPECT_GT(real_pid, 0);
    EXPECT_NE(real_pid, 42);  // very unlikely to actually be 42
}

// Test 3: Scoped bypass_mocks object
TEST_F(BypassTest, ScopedBypass)
{
    ON_MODULE_FUNC_CALL(getpid)
        .WillByDefault(Return(99));

    // Mock active
    EXPECT_EQ(getpid(), 99);

    // Scoped bypass
    {
        const gmock_linux::bypass_mocks useRealAPI{ };
        pid_t real_pid = getpid();
        EXPECT_GT(real_pid, 0);
    }

    // Mock active again after scope ends
    EXPECT_EQ(getpid(), 99);
}

// Test 4: REAL_MODULE_FUNC gives access to original function
TEST_F(BypassTest, RealModuleFuncPtr)
{
    ON_MODULE_FUNC_CALL(getpid)
        .WillByDefault(Return(77));

    // Call through mock
    EXPECT_EQ(getpid(), 77);

    // Call through real function pointer
    auto real_getpid = REAL_MODULE_FUNC(getpid);
    EXPECT_NE(real_getpid, nullptr);

    pid_t real_pid = real_getpid();
    EXPECT_GT(real_pid, 0);
}
