/**
 * vim: set ts=4 :
 * ======================================================
 * Metamod:Source
 * Copyright (C) 2004-2008 AlliedModders LLC and authors.
 * All rights reserved.
 * ======================================================
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from 
 * the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose, 
 * including commercial applications, and to alter it and redistribute it 
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not 
 * claim that you wrote the original software. If you use this software in a 
 * product, an acknowledgment in the product documentation would be 
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* 
 * Original file: http://hg.alliedmods.net/mmsource-central/file/eeea4ed7c45d/loader/utility.h
 * Changes: Removed unneeded functions.
 */

#ifndef _INCLUDE_METAMOD_SOURCE_LOADER_UTILITY_H_
#define _INCLUDE_METAMOD_SOURCE_LOADER_UTILITY_H_

#include <stddef.h>

extern size_t
mm_Format(char *buffer, size_t maxlength, const char *fmt, ...);

extern void
mm_TrimLeft(char *buffer);

extern void
mm_TrimRight(char *buffer);

extern void
mm_TrimComments(char *buffer);

extern void
mm_KeySplit(const char *str, char *buf1, size_t len1, char *buf2, size_t len2);

#endif /* _INCLUDE_METAMOD_SOURCE_LOADER_UTILITY_H_ */