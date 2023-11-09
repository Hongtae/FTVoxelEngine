#pragma once

#define FVCORE

// Build Configuration
#if defined(_DEBUG) || defined(DEBUG)
#   ifndef FVCORE_DEBUG_ENABLED
#       define FVCORE_DEBUG_ENABLED 1
#   endif
#endif

// WIN32
#ifdef _WIN32
#   define FVCORE_WIN32 1
#   include <SDKDDKVer.h>
#   if !defined(FVCORE_STATIC) && !defined(FVCORE_DYNAMIC)
#       define FVCORE_DYNAMIC 1	// DLL is default on Win32
#   endif
#   ifdef FVCORE_DYNAMIC			// DLL
#       pragma message("Build DK for Win32 DLL.")
#       ifdef FVCORE_EXPORTS
#           define FVCORE_API	__declspec(dllexport)
#       else
#           define FVCORE_API	__declspec(dllimport)
#       endif
#   else						// static
#       define FVCORE_API
#   endif	// ifdef FVCORE_DYNAMIC
#endif	// ifdef _WIN32

// APPLE OSX, iOS
#if defined(__APPLE__) && defined(__MACH__)
#   define FVCORE_APPLE_MACH
#   include <TargetConditionals.h>
#   if !defined(FVCORE_STATIC) && !defined(FVCORE_DYNAMIC)
#       define FVCORE_DYNAMIC 1	// dylib(Framework) is default.
#   endif
#   if TARGET_OS_IPHONE
#       define FVCORE_APPLE_IOS 1
#       ifdef FVCORE_DYNAMIC		// dylib or Framework
#           define FVCORE_API	__attribute__((visibility ("default")))
#       else
#           define FVCORE_API
#       endif
#   else	//if TARGET_OS_IPHONE
#       define FVCORE_APPLE_OSX 1
#       ifdef FVCORE_DYNAMIC		// dylib or Framework
#           define FVCORE_API	__attribute__((visibility ("default")))
#       else
#           define FVCORE_API
#       endif
#   endif
#endif //if defined(__APPLE__) && defined(__MACH__)

// LINUX, ANDROID
#ifdef __linux__
#   include <sys/endian.h>
#   if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#       if __BYTE_ORDER == __LITTLE_ENDIAN
#           define __LITTLE_ENDIAN__ 1
#       elif __BYTE_ORDER == __BIG_ENDIAN
#           define __BIG_ENDIAN__ 1
#       endif
#   endif
#   define FVCORE_LINUX 1

// no default library on Linux. You should define static or dynamic.
#   if !defined(FVCORE_STATIC) && !defined(FVCORE_DYNAMIC)
#       error "You should define FVCORE_STATIC or FVCORE_DYNAMIC"
#   endif

#   ifdef FVCORE_DYNAMIC		// so
#       define FVCORE_API	__attribute__((visibility ("default")))
#   else
#       define FVCORE_API
#   endif
#endif

// Unsupported OS?
#if !defined(FVCORE_STATIC) && !defined(FVCORE_DYNAMIC)
#   error "You should define FVCORE_STATIC or FVCORE_DYNAMIC"
#endif

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#   if defined(_M_X64) || defined(_M_IX86) || defined(__i386__)
#       define __LITTLE_ENDIAN__	1
#   elif defined(__ARMEB__)
#       define __BIG_ENDIAN__ 1
#   else
#       error "System byte order not defined."
#   endif
#endif

#ifndef FORCEINLINE
#   ifdef FVCORE_DEBUG_ENABLED
#       define FORCEINLINE inline
#   else
#       ifdef _MSC_VER
#           define FORCEINLINE __forceinline
#       else
#           define FORCEINLINE inline __attribute__((always_inline))
#       endif
#   endif
#endif

#ifndef NOINLINE
#   ifdef _MSC_VER
#     define NOINLINE __declspec(noinline)
#   else
#     define NOINLINE __attribute__((noinline))
#   endif
#endif

#if defined(_MSC_VER)
#   define FVCORE_FUNCTION_NAME    __FUNCTION__
#elif defined(__GNUC__)
#   define FVCORE_FUNCTION_NAME    __PRETTY_FUNCTION__
#else
#   define FVCORE_FUNCTION_NAME    __func__
#endif

#ifdef __cplusplus
#include <memory>
#include <utility>
#include <stdexcept>
#include <type_traits>
#include <algorithm>
#include <optional>
#include <functional>
#include <numeric>
#include <limits>
#include <concepts>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <format>
#include <string>
#include <cmath>

#ifdef _MSC_VER
#define FV_TRAP() __debugbreak()
#else
#define FV_TRAP() __builtin_trap()
#endif

#define _FVASSERT_THROW_EXCEPTION 0

#define FVCORE_NOOP (void)0 // Forces the macro to end the statement with a semicolon.
#define FVERROR_LOG(desc)       {std::cerr << desc << std::endl;} FVCORE_NOOP
#define FVERROR_THROW(desc)     {throw std::runtime_error(desc);} FVCORE_NOOP
#define FVERROR_ABORT(desc)     {FVERROR_LOG(desc); FV_TRAP(); abort(); } FVCORE_NOOP
#if _FVASSERT_THROW_EXCEPTION
#   define FVASSERT_DESC(expr, desc)    {if (!(expr)) FVERROR_THROW(desc);} FVCORE_NOOP
#   define FVASSERT(expr)               {if (!(expr)) FVERROR_THROW("");} FVCORE_NOOP
#else
#   define FVASSERT_DESC(expr, desc)    {if (!(expr)) FVERROR_ABORT(desc);} FVCORE_NOOP
#   define FVASSERT(expr)               {if (!(expr)) FVERROR_ABORT("");} FVCORE_NOOP
#endif

#define _FVSTRINGIFY(x) #x
#define _FVSTR(x) _FVSTRINGIFY(x)

#ifdef FVCORE_DEBUG_ENABLED
#   define FVERROR_THROW_DEBUG(desc)        FVERROR_THROW(desc)
#   define FVASSERT_DESC_DEBUG(expr, desc)  FVASSERT_DESC(expr, desc)
#   define FVASSERT_DEBUG(expr)             FVASSERT(expr)
#   define FVASSERT_THROW(expr)             {if (!(expr)) FVERROR_THROW("assertion failure: <" __FILE__ ":" _FVSTR(__LINE__) "> \nexpression: " #expr);} FVCORE_NOOP
#else
#   define FVERROR_THROW_DEBUG(desc)        FVCORE_NOOP
#   define FVASSERT_DESC_DEBUG(expr, desc)  FVCORE_NOOP
#   define FVASSERT_DEBUG(expr)             FVCORE_NOOP
#   define FVASSERT_THROW(expr)             FVCORE_NOOP
#endif
#endif	// #ifdef __cplusplus
