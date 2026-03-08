cmake_minimum_required(VERSION 3.14)
project(gmock-module VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Fetch GoogleTest
# ============================================================================

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ============================================================================
# gmock-module library
# ============================================================================

if(WIN32)
    # Windows: IAT patching (gmock-win32)
    add_library(gmock-module STATIC
        src/gmock-win32.cpp
    )
    target_include_directories(gmock-module PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(gmock-module PUBLIC
        gmock
        gtest
        dbghelp
    )

elseif(UNIX AND NOT APPLE)
    # Linux: PLT/GOT patching (gmock-linux)
    add_library(gmock-module STATIC
        src/gmock-linux.cpp
    )
    target_include_directories(gmock-module PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(gmock-module PUBLIC
        gmock
        gtest
        dl      # for dlsym, dl_iterate_phdr
    )

else()
    message(FATAL_ERROR "Unsupported platform. Only Windows and Linux are supported.")
endif()

# ============================================================================
# Tests
# ============================================================================

enable_testing()

if(UNIX AND NOT APPLE)
    # Linux tests
    add_executable(gmock_module_tests
        tests/test_main.cpp
        tests/test_open.cpp
        tests/test_read_write.cpp
        tests/test_stat.cpp
        tests/test_bypass.cpp
    )

    target_link_libraries(gmock_module_tests PRIVATE
        gmock-module
        gtest
        gmock
    )

    # Use lazy binding to ensure GOT is patchable
    target_link_options(gmock_module_tests PRIVATE
        -Wl,-z,lazy
        -rdynamic
    )

    # Ensure the test binary is not PIE-optimized away from GOT usage
    # (most modern distros default to PIE, which is fine — GOT is still used)

    add_test(NAME gmock_module_tests COMMAND gmock_module_tests)

elseif(WIN32)
    # Windows tests (using original gmock-win32 API)
    add_executable(gmock_module_tests
        tests/test_main_win32.cpp
    )

    target_link_libraries(gmock_module_tests PRIVATE
        gmock-module
        gtest
        gmock
    )

    add_test(NAME gmock_module_tests COMMAND gmock_module_tests)
endif()
