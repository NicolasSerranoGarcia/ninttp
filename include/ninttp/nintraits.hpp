#pragma once

#include <cstdint>

#if defined(__has_include)
    #if __has_include(<ninttp/ninttp_platform_config.hpp>)
        #include <ninttp/ninttp_platform_config.hpp>
    #endif
#endif

//the name has a prefix to avoid naming issues with system includes

//defines endianness utilities at compile time

//defines NINTTP_BYTE_ORDER to be NINTTP_LITTLE_ENDIAN or NINTTP_BIG_ENDIAN
//depending on what is the host endianness

//Always defined when including ninendian.hpp
#define NINTTP_BIG_ENDIAN 4321
//Always defined when including ninendian.hpp
#define NINTTP_LITTLE_ENDIAN 1234

#if !defined(NINTTP_BYTE_ORDER) && (defined __unix__ || defined __unix || defined __APPLE__ || defined(__MACH__))
    #include <sys/param.h>
#endif

#ifndef NINTTP_BYTE_ORDER
    #if defined __BYTE_ORDER
        #if __BYTE_ORDER == __LITTLE_ENDIAN
            #define NINTTP_BYTE_ORDER NINTTP_LITTLE_ENDIAN
        #elif __BYTE_ORDER == __BIG_ENDIAN
            #define NINTTP_BYTE_ORDER NINTTP_BIG_ENDIAN
        #endif
    #endif
#endif

#ifndef NINTTP_BYTE_ORDER
    #if defined __BYTE_ORDER__
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            #define NINTTP_BYTE_ORDER NINTTP_LITTLE_ENDIAN
        #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            #define NINTTP_BYTE_ORDER NINTTP_BIG_ENDIAN
        #endif
    #endif
#endif

#ifndef NINTTP_BYTE_ORDER
    #if defined(_WIN32) || defined(_WIN64) || defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)
        #define NINTTP_BYTE_ORDER NINTTP_LITTLE_ENDIAN
    #elif defined(__i386__) || defined(__x86_64__) || defined(__AARCH64EL__) || defined(__ARMEL__) || defined(__MIPSEL__)
        #define NINTTP_BYTE_ORDER NINTTP_LITTLE_ENDIAN
    #elif defined(__AARCH64EB__) || defined(__ARMEB__) || defined(__MIPSEB__)
        #define NINTTP_BYTE_ORDER NINTTP_BIG_ENDIAN
    #endif
#endif

#ifndef NINTTP_BYTE_ORDER
    #error "ninttp: Unable to detect host byte order. Define NINTTP_BYTE_ORDER to NINTTP_LITTLE_ENDIAN or NINTTP_BIG_ENDIAN."
#endif

#ifdef NINTTP_BYTE_ORDER
    #define NINTTP_BYTE_ORDER_DEFINED 1
#endif

//define system flags to know in which platform are we. e.g. (notion):
//NINTTP_WINDOWS
//NINTTP_UNIX_COMPLIANT
//define flags that may change how the API behaves (for example, if the backend between platform A and platform B does not change, then there is no need to add a flag that specifies
//a difference between both)

#if !defined(NINTTP_PLATFORM_WINDOWS) && !defined(NINTTP_PLATFORM_UNIX) && !defined(NINTTP_PLATFORM_UNKNOWN)
    #if defined(_WIN32) || defined(_WIN64)
        #define NINTTP_PLATFORM_WINDOWS 1
    #elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__MACH__) || defined(__linux__) || defined(__ANDROID__)
        #define NINTTP_PLATFORM_UNIX 1
    #else
        #define NINTTP_PLATFORM_UNKNOWN 1
    #endif
#endif

//always set the flag to 0 either when: (both cmake configuration is not used and we are not on windows) or (cmake configuration is used but we are not on windows) 
#ifndef NINTTP_PLATFORM_WINDOWS
    #define NINTTP_PLATFORM_WINDOWS 0
#endif
//same for the rest
#ifndef NINTTP_PLATFORM_UNIX
    #define NINTTP_PLATFORM_UNIX 0
#endif

#ifndef NINTTP_PLATFORM_UNKNOWN
    #define NINTTP_PLATFORM_UNKNOWN 0
#endif

// Optional platform detail flags
#if !defined(NINTTP_PLATFORM_LINUX) && defined(__linux__)
    #define NINTTP_PLATFORM_LINUX 1
#endif

#if !defined(NINTTP_PLATFORM_APPLE) && defined(__APPLE__) && defined(__MACH__)
    #define NINTTP_PLATFORM_APPLE 1
#endif

#if !defined(NINTTP_PLATFORM_ANDROID) && defined(__ANDROID__)
    #define NINTTP_PLATFORM_ANDROID 1
#endif

#if !defined(NINTTP_PLATFORM_BSD) && (defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
    #define NINTTP_PLATFORM_BSD 1
#endif

#ifndef NINTTP_PLATFORM_LINUX
    #define NINTTP_PLATFORM_LINUX 0
#endif

#ifndef NINTTP_PLATFORM_APPLE
    #define NINTTP_PLATFORM_APPLE 0
#endif

#ifndef NINTTP_PLATFORM_ANDROID
    #define NINTTP_PLATFORM_ANDROID 0
#endif

#ifndef NINTTP_PLATFORM_BSD
    #define NINTTP_PLATFORM_BSD 0
#endif

// Backend/behavior traits
#if !defined(NINTTP_BACKEND_WINSOCK) && !defined(NINTTP_BACKEND_POSIX)
    #if NINTTP_PLATFORM_WINDOWS == 1
        #define NINTTP_BACKEND_WINSOCK 1
        #define NINTTP_SOCKET_BACKEND_REQUIRES_INIT 1
        #define NINTTP_SOCKET_INVALID_HANDLE_ALL_BITS_ONE 1
    #elif NINTTP_PLATFORM_UNIX == 1
        //fallback only; prefer CMake feature detection for backend selection
        #define NINTTP_BACKEND_POSIX 1
        #define NINTTP_SOCKET_BACKEND_REQUIRES_INIT 0
        #define NINTTP_SOCKET_INVALID_HANDLE_MINUS_ONE 1
    #endif
#endif

#ifndef NINTTP_BACKEND_WINSOCK
    #define NINTTP_BACKEND_WINSOCK 0
#endif

#ifndef NINTTP_BACKEND_POSIX
    #define NINTTP_BACKEND_POSIX 0
#endif

#ifndef NINTTP_SOCKET_BACKEND_REQUIRES_INIT
    #define NINTTP_SOCKET_BACKEND_REQUIRES_INIT 0
#endif

#ifndef NINTTP_SOCKET_INVALID_HANDLE_ALL_BITS_ONE
    #define NINTTP_SOCKET_INVALID_HANDLE_ALL_BITS_ONE 0
#endif

#ifndef NINTTP_SOCKET_INVALID_HANDLE_MINUS_ONE
    #define NINTTP_SOCKET_INVALID_HANDLE_MINUS_ONE 0
#endif

#ifndef NINTTP_FD_CLOSE_EXEC
    #define NINTTP_FD_CLOSE_EXEC 1
#endif

#ifndef NINTTP_HAVE_ACCEPT4
    #define NINTTP_HAVE_ACCEPT4 0
#endif
