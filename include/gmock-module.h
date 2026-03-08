// gmock-module.h — Cross-platform header for mocking system API functions.
//
// On Windows: uses IAT (Import Address Table) patching via gmock-win32
// On Linux:   uses PLT/GOT patching via gmock-linux
//
// Usage:
//   #include <gmock-module.h>
//
//   // Works on both platforms:
//   MOCK_MODULE_FUNC(int, open, const char*, int);
//   EXPECT_MODULE_FUNC_CALL(open, testing::_, testing::_).WillOnce(Return(42));
//
// License: MIT

#pragma once

#if defined(_WIN32)
    #include "gmock-win32.h"

    // Map gmock_module namespace to gmock_win32 for cross-platform code
    namespace gmock_module = gmock_win32;

#elif defined(__linux__)
    #include "gmock-linux.h"

    // Map gmock_module namespace to gmock_linux for cross-platform code
    namespace gmock_module = gmock_linux;

#else
    #error "gmock-module: unsupported platform. Only Windows and Linux are supported."
#endif
