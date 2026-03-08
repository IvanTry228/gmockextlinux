// gmock-linux.h — GMock extension for mocking Linux system API functions
// via PLT/GOT patching. Adapted from gmock-win32 by smalti.
//
// License: MIT

#pragma once

#include <gmock/gmock.h>
#include <stdexcept>
#include <memory>
#include <type_traits>

// Optimization control for GCC/Clang
#define GMOCK_LINUX_PRAGMA_UNOPTIMIZE_ON
#define GMOCK_LINUX_PRAGMA_UNOPTIMIZE_OFF
#define GMOCK_LINUX_ATTRIBUTE_UNOPTIMIZE __attribute__((optimize("O0")))

// GMock internal helpers — use prefixed names to avoid conflicts with gmock's own macros
#define GMOCK_LINUX_RESULT_(tn, ...) \
    tn ::testing::internal::Function<__VA_ARGS__>::Result

#define GMOCK_LINUX_ARG_(tn, N, ...) \
    tn ::testing::internal::Function<__VA_ARGS__>::template Arg< N-1 >::type

#define GMOCK_LINUX_MATCHER_(tn, N, ...) \
    const ::testing::Matcher< GMOCK_LINUX_ARG_(tn, N, __VA_ARGS__) >&

#define GMOCK_LINUX_MOCKER_(arity, constness, func) \
    GTEST_CONCAT_TOKEN_(gmock##constness##arity##_##func##_, __LINE__)

// ============================================================================
// Namespace declarations
// ============================================================================

namespace gmock_linux {

    void initialize();
    void uninitialize() noexcept;

    struct init_scope
    {
        init_scope()  { initialize(); }
        ~init_scope() { uninitialize(); }
    };

    struct bypass_mocks final
    {
        bypass_mocks()  noexcept;
        ~bypass_mocks() noexcept;
    };

namespace detail {

    extern thread_local int lock;

    struct proxy_base { ~proxy_base() noexcept; };

    template< typename Reference >
    struct ref_proxy final : proxy_base
    {
        explicit ref_proxy(Reference&& r) noexcept : ref_{ r } { }
        operator Reference() const noexcept { return ref_; }

    private:
        Reference ref_;
    };

    template< typename Reference >
    ref_proxy< Reference > make_proxy(Reference&& r) noexcept
    {
        return ref_proxy< Reference >{ std::forward< Reference >(r) };
    }

    template< typename Derived >
    struct mock_module_base
    {
        static Derived& instance()
        {
            static ::testing::template NiceMock< Derived > obj;
            return obj;
        }

        static void** pp_old_fn()
        {
            static void* oldFn = nullptr;
            return &oldFn;
        }

        static bool& has_expectations()
        {
            static thread_local bool value = false;
            return value;
        }

        static void set_expectations(bool value)
        {
            has_expectations() = value;
        }
    };

    void patch_module_func   (const char*, void*, void*, void**);
    void restore_module_func (const char*, void*, void*, void**);

    template< typename TFunc, typename TStub >
    void patch_module_func_non_optimized(
        const char* func_name, void** old_fn, TFunc func, TStub stub)
    {
        if (!(*old_fn))
            patch_module_func(
                func_name, reinterpret_cast< void* >(func), reinterpret_cast< void* >(stub), old_fn);
    }

} // namespace detail
} // namespace gmock_linux

// ============================================================================
// Bypass mocks macro
// ============================================================================

#define BYPASS_MOCKS(expr) \
    do \
    { \
        const gmock_linux::bypass_mocks block_mocks{ }; \
        expr; \
    } \
    while (false);

// ============================================================================
// Mock struct generation macros (0 to 6 arguments)
// Linux uses cdecl (System V ABI) — no calling convention specifier needed
// ============================================================================

#define MOCK_MODULE_FUNC0_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func() \
        constness \
    { \
        GMOCK_LINUX_MOCKER_(0, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(0, constness, func).Invoke(); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func() \
    { \
        GMOCK_LINUX_MOCKER_(0, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(0, constness, func).With(); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(0, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub() \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())(); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func(); \
            } \
            else \
            { \
                return instance().func(); \
            } \
        } \
    } \
};

#define MOCK_MODULE_FUNC1_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1) constness \
    { \
        GMOCK_LINUX_MOCKER_(1, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(1, constness, func).Invoke( \
            gmock_a1); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func( \
        GMOCK_LINUX_MATCHER_(tn, 1, __VA_ARGS__) gmock_a1) constness \
    { \
        GMOCK_LINUX_MOCKER_(1, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(1, constness, func).With( \
            gmock_a1); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(1, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1) \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())( \
                gmock_a1); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func( \
                    gmock_a1); \
            } \
            else \
            { \
                return instance().func( \
                    gmock_a1); \
            } \
        } \
    } \
};

#define MOCK_MODULE_FUNC2_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2) constness \
    { \
        GMOCK_LINUX_MOCKER_(2, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(2, constness, func).Invoke( \
            gmock_a1, gmock_a2); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func( \
        GMOCK_LINUX_MATCHER_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_MATCHER_(tn, 2, __VA_ARGS__) gmock_a2) constness \
    { \
        GMOCK_LINUX_MOCKER_(2, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(2, constness, func).With( \
            gmock_a1, gmock_a2); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(2, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2) \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())( \
                gmock_a1, gmock_a2); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func( \
                    gmock_a1, gmock_a2); \
            } \
            else \
            { \
                return instance().func( \
                    gmock_a1, gmock_a2); \
            } \
        } \
    } \
};

#define MOCK_MODULE_FUNC3_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3) constness \
    { \
        GMOCK_LINUX_MOCKER_(3, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(3, constness, func).Invoke( \
            gmock_a1, gmock_a2, gmock_a3); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func( \
        GMOCK_LINUX_MATCHER_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_MATCHER_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_MATCHER_(tn, 3, __VA_ARGS__) gmock_a3) constness \
    { \
        GMOCK_LINUX_MOCKER_(3, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(3, constness, func).With( \
            gmock_a1, gmock_a2, gmock_a3); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(3, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3) \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())( \
                gmock_a1, gmock_a2, gmock_a3); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3); \
            } \
            else \
            { \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3); \
            } \
        } \
    } \
};

#define MOCK_MODULE_FUNC4_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_ARG_(tn, 4, __VA_ARGS__) gmock_a4) constness \
    { \
        GMOCK_LINUX_MOCKER_(4, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(4, constness, func).Invoke( \
            gmock_a1, gmock_a2, gmock_a3, gmock_a4); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func( \
        GMOCK_LINUX_MATCHER_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_MATCHER_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_MATCHER_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_MATCHER_(tn, 4, __VA_ARGS__) gmock_a4) constness \
    { \
        GMOCK_LINUX_MOCKER_(4, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(4, constness, func).With( \
            gmock_a1, gmock_a2, gmock_a3, gmock_a4); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(4, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_ARG_(tn, 4, __VA_ARGS__) gmock_a4) \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())( \
                gmock_a1, gmock_a2, gmock_a3, gmock_a4); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3, gmock_a4); \
            } \
            else \
            { \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3, gmock_a4); \
            } \
        } \
    } \
};

#define MOCK_MODULE_FUNC5_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_ARG_(tn, 4, __VA_ARGS__) gmock_a4, \
        GMOCK_LINUX_ARG_(tn, 5, __VA_ARGS__) gmock_a5) constness \
    { \
        GMOCK_LINUX_MOCKER_(5, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(5, constness, func).Invoke( \
            gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func( \
        GMOCK_LINUX_MATCHER_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_MATCHER_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_MATCHER_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_MATCHER_(tn, 4, __VA_ARGS__) gmock_a4, \
        GMOCK_LINUX_MATCHER_(tn, 5, __VA_ARGS__) gmock_a5) constness \
    { \
        GMOCK_LINUX_MOCKER_(5, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(5, constness, func).With( \
            gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(5, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_ARG_(tn, 4, __VA_ARGS__) gmock_a4, \
        GMOCK_LINUX_ARG_(tn, 5, __VA_ARGS__) gmock_a5) \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())( \
                gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5); \
            } \
            else \
            { \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5); \
            } \
        } \
    } \
};

#define MOCK_MODULE_FUNC6_IMPL_(tn, constness, ct, func, use_locks, ...) \
struct mock_module_##func : \
    gmock_linux::detail::mock_module_base< mock_module_##func > \
{ \
    GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct func( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_ARG_(tn, 4, __VA_ARGS__) gmock_a4, \
        GMOCK_LINUX_ARG_(tn, 5, __VA_ARGS__) gmock_a5, \
        GMOCK_LINUX_ARG_(tn, 6, __VA_ARGS__) gmock_a6) constness \
    { \
        GMOCK_LINUX_MOCKER_(6, constness, func).SetOwnerAndName(this, #func); \
        return GMOCK_LINUX_MOCKER_(6, constness, func).Invoke( \
            gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5, gmock_a6); \
    } \
    ::testing::MockSpec<__VA_ARGS__> gmock_##func( \
        GMOCK_LINUX_MATCHER_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_MATCHER_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_MATCHER_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_MATCHER_(tn, 4, __VA_ARGS__) gmock_a4, \
        GMOCK_LINUX_MATCHER_(tn, 5, __VA_ARGS__) gmock_a5, \
        GMOCK_LINUX_MATCHER_(tn, 6, __VA_ARGS__) gmock_a6) constness \
    { \
        GMOCK_LINUX_MOCKER_(6, constness, func).RegisterOwner(this); \
        return GMOCK_LINUX_MOCKER_(6, constness, func).With( \
            gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5, gmock_a6); \
    } \
    mutable ::testing::FunctionMocker<__VA_ARGS__> \
        GMOCK_LINUX_MOCKER_(6, constness, func); \
    static GMOCK_LINUX_RESULT_(tn, __VA_ARGS__) ct stub( \
        GMOCK_LINUX_ARG_(tn, 1, __VA_ARGS__) gmock_a1, \
        GMOCK_LINUX_ARG_(tn, 2, __VA_ARGS__) gmock_a2, \
        GMOCK_LINUX_ARG_(tn, 3, __VA_ARGS__) gmock_a3, \
        GMOCK_LINUX_ARG_(tn, 4, __VA_ARGS__) gmock_a4, \
        GMOCK_LINUX_ARG_(tn, 5, __VA_ARGS__) gmock_a5, \
        GMOCK_LINUX_ARG_(tn, 6, __VA_ARGS__) gmock_a6) \
    { \
        if (!has_expectations() || gmock_linux::detail::lock) \
        { \
            return reinterpret_cast< decltype(&stub) >(*pp_old_fn())( \
                gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5, gmock_a6); \
        } \
        else \
        { \
            if (use_locks) \
            { \
                const gmock_linux::bypass_mocks block_mocks{ }; \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5, gmock_a6); \
            } \
            else \
            { \
                return instance().func( \
                    gmock_a1, gmock_a2, gmock_a3, gmock_a4, gmock_a5, gmock_a6); \
            } \
        } \
    } \
};

// ============================================================================
// Argument counting and dispatch macros
// ============================================================================

// Token concatenation helpers
#define GMOCK_LINUX_CAT_(a, b) a##b
#define GMOCK_LINUX_CAT2_(a, b) GMOCK_LINUX_CAT_(a, b)

// Expand helper — unpacks parens
#define GMOCK_LINUX_EXPAND_(...) __VA_ARGS__

// Non-optimized patch function generator
#define GMOCK_LINUX_PATCH_FUNC_(m) \
    GMOCK_LINUX_PRAGMA_UNOPTIMIZE_ON \
    static void GMOCK_LINUX_ATTRIBUTE_UNOPTIMIZE patch_module_func_##m() { \
        gmock_linux::detail::patch_module_func_non_optimized( \
            #m, mock_module_##m::pp_old_fn(), &::m, &mock_module_##m::stub); \
    } \
    GMOCK_LINUX_PRAGMA_UNOPTIMIZE_OFF

// Dispatch: select a MOCK_MODULE_FUNCn_IMPL_ macro + generate patch function.
// Uses indirect call to ensure full expansion before the IMPL_ macro
// counts its arguments.
#define GMOCK_LINUX_DISPATCH_APPLY_(macro, ...) macro(__VA_ARGS__)

// Per-arity direct macros — these construct the function type directly.
//   r           = return type
//   use_locks   = true/false
//   m           = function name
//   arg types...

#define GMOCK_LINUX_MOCK_0_(r, use_locks, m) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC0_IMPL_, , , , m, use_locks, r()) \
    GMOCK_LINUX_PATCH_FUNC_(m)

#define GMOCK_LINUX_MOCK_1_(r, use_locks, m, a1) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC1_IMPL_, , , , m, use_locks, r(a1)) \
    GMOCK_LINUX_PATCH_FUNC_(m)

#define GMOCK_LINUX_MOCK_2_(r, use_locks, m, a1, a2) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC2_IMPL_, , , , m, use_locks, r(a1, a2)) \
    GMOCK_LINUX_PATCH_FUNC_(m)

#define GMOCK_LINUX_MOCK_3_(r, use_locks, m, a1, a2, a3) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC3_IMPL_, , , , m, use_locks, r(a1, a2, a3)) \
    GMOCK_LINUX_PATCH_FUNC_(m)

#define GMOCK_LINUX_MOCK_4_(r, use_locks, m, a1, a2, a3, a4) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC4_IMPL_, , , , m, use_locks, r(a1, a2, a3, a4)) \
    GMOCK_LINUX_PATCH_FUNC_(m)

#define GMOCK_LINUX_MOCK_5_(r, use_locks, m, a1, a2, a3, a4, a5) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC5_IMPL_, , , , m, use_locks, r(a1, a2, a3, a4, a5)) \
    GMOCK_LINUX_PATCH_FUNC_(m)

#define GMOCK_LINUX_MOCK_6_(r, use_locks, m, a1, a2, a3, a4, a5, a6) \
    GMOCK_LINUX_DISPATCH_APPLY_(MOCK_MODULE_FUNC6_IMPL_, , , , m, use_locks, r(a1, a2, a3, a4, a5, a6)) \
    GMOCK_LINUX_PATCH_FUNC_(m)

// VA_ARGS overload selector — picks the right GMOCK_LINUX_MOCK_N_ based on argument count
// Args:   m, [a1, ..., a6], _LAST_
// Total: func_name + 0..6 arg types  = 1..7 positional args, plus the sentinel _LAST_
#define GMOCK_LINUX_GET_MACRO_(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME

// ============================================================================
// Public API macros
// ============================================================================

// Create mock: MOCK_MODULE_FUNC(return_type, func_name, arg_type1, arg_type2, ...)
// The variadic args are: func_name, arg_types...
#define MOCK_MODULE_FUNC(r, ...) \
    GMOCK_LINUX_GET_MACRO_(__VA_ARGS__, \
        GMOCK_LINUX_MOCK_6_, GMOCK_LINUX_MOCK_5_, GMOCK_LINUX_MOCK_4_, \
        GMOCK_LINUX_MOCK_3_, GMOCK_LINUX_MOCK_2_, GMOCK_LINUX_MOCK_1_, \
        GMOCK_LINUX_MOCK_0_)(r, false, __VA_ARGS__)

#define MOCK_MODULE_FUNC_WITH_BYPASS(r, ...) \
    GMOCK_LINUX_GET_MACRO_(__VA_ARGS__, \
        GMOCK_LINUX_MOCK_6_, GMOCK_LINUX_MOCK_5_, GMOCK_LINUX_MOCK_4_, \
        GMOCK_LINUX_MOCK_3_, GMOCK_LINUX_MOCK_2_, GMOCK_LINUX_MOCK_1_, \
        GMOCK_LINUX_MOCK_0_)(r, true, __VA_ARGS__)

// Aliases for cross-platform compatibility with gmock-win32
#define MOCK_CDECL_FUNC(r, ...)               MOCK_MODULE_FUNC(r, __VA_ARGS__)
#define MOCK_CDECL_FUNC_WITH_BYPASS(r, ...)   MOCK_MODULE_FUNC_WITH_BYPASS(r, __VA_ARGS__)
#define MOCK_STDCALL_FUNC(r, ...)             MOCK_MODULE_FUNC(r, __VA_ARGS__)
#define MOCK_STDCALL_FUNC_WITH_BYPASS(r, ...) MOCK_MODULE_FUNC_WITH_BYPASS(r, __VA_ARGS__)

// ============================================================================
// Expectations
// ============================================================================

#define MODULE_FUNC_CALL_IMPL_(EXPECTATION_, func, ...) \
    patch_module_func_##func(); \
    ++gmock_linux::detail::lock; \
    mock_module_##func::set_expectations(true); \
    static_cast< decltype(EXPECTATION_(mock_module_##func::instance(), \
        func(__VA_ARGS__)))& >(gmock_linux::detail::make_proxy( \
            EXPECTATION_(mock_module_##func::instance(), func(__VA_ARGS__))))

#define EXPECT_MODULE_FUNC_CALL(func, ...) \
    MODULE_FUNC_CALL_IMPL_(EXPECT_CALL, func, __VA_ARGS__)

#define ON_MODULE_FUNC_CALL(func, ...) \
    MODULE_FUNC_CALL_IMPL_(ON_CALL, func, __VA_ARGS__)

// ============================================================================
// Real function call
// ============================================================================

#define REAL_MODULE_FUNC_IMPL_(func) \
    reinterpret_cast< decltype(&func) >(*mock_module_##func::pp_old_fn())

#define REAL_MODULE_FUNC(func) \
    REAL_MODULE_FUNC_IMPL_(func)

#define INVOKE_REAL_MODULE_FUNC(func, ...) \
    REAL_MODULE_FUNC(func)(__VA_ARGS__)

// ============================================================================
// Verifying and removing expectations
// ============================================================================

#define VERIFY_AND_CLEAR_MODULE_FUNC_IMPL_(func) \
    mock_module_##func::set_expectations(false); \
    ::testing::Mock::VerifyAndClear(&mock_module_##func::instance())

#define VERIFY_AND_CLEAR_MODULE_FUNC(func) \
    VERIFY_AND_CLEAR_MODULE_FUNC_IMPL_(func)

#define VERIFY_AND_CLEAR_MODULE_FUNC_EXPECTATIONS_IMPL_(func) \
    mock_module_##func::set_expectations(false); \
    ::testing::Mock::VerifyAndClearExpectations(&mock_module_##func::instance())

#define VERIFY_AND_CLEAR_MODULE_FUNC_EXPECTATIONS(func) \
    VERIFY_AND_CLEAR_MODULE_FUNC_EXPECTATIONS_IMPL_(func)

// ============================================================================
// Functions restoring
// ============================================================================

#define RESTORE_MODULE_FUNC_IMPL_(func) \
    gmock_linux::detail::restore_module_func( \
        #func, *mock_module_##func::pp_old_fn(), \
        reinterpret_cast< void* >(mock_module_##func::stub), \
        mock_module_##func::pp_old_fn())

#define RESTORE_MODULE_FUNC(func) \
    RESTORE_MODULE_FUNC_IMPL_(func)
