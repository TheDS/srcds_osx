/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server NG
 * Copyright (C) 2011-2013 Scott Ehlert and AlliedModders LLC.
 * All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "Steamworks SDK," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.
 */

#ifndef _INCLUDE_GAMEAPI_PLATFORM_H_
#define _INCLUDE_GAMEAPI_PLATFORM_H_

// OS definitions
#if defined(WIN32) || defined(_WIN32)
    #if !defined(PLATFORM_WINDOWS)
        #define PLATFORM_WINDOWS
    #endif

    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <windows.h>
    
    #if defined(WIN64) || defined(_WIN64)
        #if !defined(PLATFORM_X64)
            #define PLATFORM_X64
        #endif
    #else
        #if !defined(PLATFORM_X86)
            #define PLATFORM_X86
        #endif
    #endif    
#elif defined(__linux__) || defined(__APPLE__)
    #if defined(__linux__) && !defined(PLATFORM_LINUX)
        #define PLATFORM_LINUX
    #elif defined(__APPLE__) && !defined(PLATFORM_MACOSX)
        #define PLATFORM_MACOSX
    #endif

    #if !defined(PLATFORM_POSIX)
        #define PLATFORM_POSIX
    #endif
    
    #if defined(__x86_64__)
        #if !defined(PLATFORM_X64)
            #define PLATFORM_X64
        #endif
    #else
        #if !defined(PLATFORM_X86)
            #define PLATFORM_X86
        #endif
    #endif
#else
    #error "Unsupported platform"
#endif

// Compiler definitions
#if defined(_MSC_VER)
#if !defined(COMPILER_MSVC)
    #define COMPILER_MSVC
#endif
#elif defined(__clang__)
#if !defined(COMPILER_CLANG)
    #define COMPILER_CLANG
#endif
#elif defined(__GNUC__)
#if !defined(COMPILER_GCC)
    #define COMPILER_GCC
#endif
#else
    #error "Unsupported compiler"
#endif

// Function export macros
#if defined(COMPILER_MSVC)
    #if defined(DLL_EXPORT)
        #define SRCDS_API extern "C" __declspec(dllexport)
    #else
        #define SRCDS_API extern "C" __declspec(dllimport)
    #endif
#else
    #define SRCDS_API extern "C" __attribute__((visibility("default")))
#endif

// Filesystem path separator as a string or character
#if defined(PLATFORM_WINDOWS)
    #define PLATFORM_SEP      "\\"
    #define PLATFORM_SEP_CHAR '\\'
#elif defined(PLATFORM_POSIX)
    #define PLATFORM_SEP      "/"
    #define PLATFORM_SEP_CHAR '/'
#endif

// Array length macros
#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0]))
#define ARRAY_END(array) ((array) + ARRAY_LENGTH(array))

#endif // _INCLUDE_GAMEAPI_PLATFORM_H_
