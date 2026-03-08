# gmock-module

Cross-platform GMock extension for mocking system API functions.

| Platform | Technique | System APIs |
|----------|-----------|-------------|
| **Windows** | IAT (Import Address Table) patching | Win32 API (`GetCurrentProcessId`, `CreateFile`, etc.) |
| **Linux** | PLT/GOT (Procedure Linkage Table / Global Offset Table) patching | POSIX API (`open`, `read`, `write`, `stat`, etc.) |

Based on [gmock-win32](https://github.com/smalti/gmock-win32) by smalti. Extended with Linux support using ELF PLT/GOT patching via `dl_iterate_phdr` + `mprotect`.

## Quick Start (Linux)

```cpp
#include <gmock-module.h>
#include <fcntl.h>
#include <gtest/gtest.h>

// 1. Declare mock in global scope
MOCK_MODULE_FUNC(int, open, const char*, int);

// 2. Write test
TEST(OpenTest, MockReturnsCustomFd) {
    EXPECT_MODULE_FUNC_CALL(open, testing::_, testing::_)
        .WillOnce(testing::Return(42));
    
    int fd = open("file.txt", O_RDONLY);
    EXPECT_EQ(fd, 42);
    
    VERIFY_AND_CLEAR_MODULE_FUNC(open);
}

// 3. Initialize in main
int main(int argc, char* argv[]) {
    const gmock_linux::init_scope init{ };
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest --verbose
```

## API Reference

| Macro | Purpose |
|-------|---------|
| `MOCK_MODULE_FUNC(ret, name, args...)` | Declare a mock for a function |
| `EXPECT_MODULE_FUNC_CALL(name, matchers...)` | Set expectation (must be called) |
| `ON_MODULE_FUNC_CALL(name, matchers...)` | Set default action |
| `VERIFY_AND_CLEAR_MODULE_FUNC(name)` | Verify and clear all expectations |
| `RESTORE_MODULE_FUNC(name)` | Restore original function |
| `BYPASS_MOCKS(expr)` | Execute expression with mocks disabled |
| `REAL_MODULE_FUNC(name)` | Get pointer to original function |
| `INVOKE_REAL_MODULE_FUNC(name, args...)` | Call original function directly |

## Known Limitations

- Only dynamically-linked library functions can be mocked (not raw `syscall` instructions)
- Not thread-safe: expectations must be set in the same thread as calls
- Full RELRO (`-z now`) may require `mprotect` override — build uses `-z lazy` by default
- Some functions like `stat()` may be macros/inlines in older glibc — use `lstat()` or `__xstat()` instead

## License

MIT
