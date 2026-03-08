// test_main.cpp — GTest entry point with gmock_linux initialization
//
// Initializes the gmock-linux PLT/GOT patching engine before running tests.

#include <gmock-module.h>
#include <gtest/gtest.h>

int main(int argc, char* argv[])
{
    // Initialize the PLT/GOT patching engine
    const gmock_linux::init_scope gmockLinux{ };

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
