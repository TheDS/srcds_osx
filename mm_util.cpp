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
 * Original file: http://hg.alliedmods.net/mmsource-central/file/eeea4ed7c45d/loader/utility.cpp
 * Changes: Removed unneeded functions.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mm_util.h"

size_t
mm_FormatArgs(char *buffer, size_t maxlength, const char *fmt, va_list params)
{
	size_t len = vsnprintf(buffer, maxlength, fmt, params);
	
	if (len >= maxlength)
	{
		len = maxlength - 1;
		buffer[len] = '\0';
	}
	
	return len;
}

size_t
mm_Format(char *buffer, size_t maxlength, const char *fmt, ...)
{
	size_t len;
	va_list ap;
	
	va_start(ap, fmt);
	len = mm_FormatArgs(buffer, maxlength, fmt, ap);
	va_end(ap);
	
	return len;
}

void
mm_TrimLeft(char *buffer)
{
	/* Let's think of this as our iterator */
	char *i = buffer;
	
	/* Make sure the buffer isn't null */
	if (i && *i)
	{
		/* Add up number of whitespace characters */
		while(isspace((unsigned char) *i))
			i++;
		
		/* If whitespace chars in buffer then adjust string so first non-whitespace char is at start of buffer */
		if (i != buffer)
			memmove(buffer, i, (strlen(i) + 1) * sizeof(char));
	}
}

void
mm_TrimRight(char *buffer)
{
	/* Make sure buffer isn't null */
	if (buffer)
	{
		size_t len = strlen(buffer);
		
		/* Loop through buffer backwards while replacing whitespace chars with null chars */
		for (size_t i = len - 1; i < len; i--)
		{
			if (isspace((unsigned char) buffer[i]))
				buffer[i] = '\0';
			else
				break;
		}
	}
}

void
mm_TrimComments(char *buffer)
{
	int num_sc = 0;
	size_t len = strlen(buffer);
	if (buffer)
	{
		for (int i = len - 1; i >= 0; i--)
		{
			if (buffer[i] == '/')
			{
				if (++num_sc >= 2 && i==0)
				{
					buffer[i] = '\0';
					return;
				}
			}
			else
			{
				if (num_sc >= 2)
				{
					buffer[i] = '\0';
					return;
				}
				num_sc = 0;
			}
			/* size_t won't go below 0, manually break out */
			if (i == 0)
				break;
			
		}
	}
}

void 
mm_KeySplit(const char *str, char *buf1, size_t len1, char *buf2, size_t len2)
{
	size_t start;
	size_t len = strlen(str);
	
	for (start = 0; start < len; start++)
	{
		if (!isspace(str[start]))
			break;
	}
	
	size_t end;
	for (end = start; end < len; end++)
	{
		if (isspace(str[end]))
			break;
	}
	
	size_t i, c = 0;
	for (i = start; i < end; i++, c++)
	{
		if (c >= len1)
			break;
		buf1[c] = str[i];
	}
	buf1[c] = '\0';
	
	for (start = end; start < len; start++)
	{
		if (!isspace(str[start]))
			break;
	}
	
	for (c = 0; start < len; start++, c++)
	{
		if (c >= len2)
			break;
		buf2[c] = str[start];
	}
	buf2[c] = '\0';
}
