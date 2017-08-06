/**
* vim: set ts=4 :
* =============================================================================
* SourceMod
* Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
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
* "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
* by the Valve Corporation.  You must obey the GNU General Public License in
* all respects for all other code used.  Additionally, AlliedModders LLC grants
* this exception to all derivative works.  AlliedModders LLC defines further
* exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
* or <http://www.sourcemod.net/license.php>.
*
* Version: $Id$
*/

#include "detours.h"
#include <asm/asm.h>

CPageAlloc GenBuffer::ms_Allocator(16);

static inline void RelativeJump32(GenBuffer &codegen, void *target)
{
	jitoffs_t call = IA32_Jump_Imm32(&codegen, 0);
	IA32_Write_Jump32_Abs(&codegen, call, target);
}

#if defined(_WIN64) || defined(__x86_64__)
static inline bool IsShortJump(GenBuffer &codegen, void *target)
{
	int64_t diff = int64_t(target) - int64_t(codegen.GetData()) + codegen.get_outputpos() + 5;
	int32_t upperBits = (diff >> 32);
	return upperBits == 0 || upperBits == -1;
}
#endif

static inline void AbsJump(GenBuffer &codegen, void *target)
{
#if defined(_WIN64) || defined(__x86_64__)
	if (IsShortJump(codegen, target))
		RelativeJump32(codegen, target);
	else
		X64_Jump_Abs(&codegen, target);
#else
	RelativeJump32(codegen, target);
#endif
}

CDetour *CDetourManager::CreateDetour(void *callbackfunction, void **trampoline, void *addr)
{
	CDetour *detour = new CDetour(callbackfunction, trampoline);
	if (detour)
	{
		if (!detour->Init(addr))
		{
			delete detour;
			return NULL;
		}

		return detour;
	}

	return NULL;
}

CDetour::CDetour(void *callbackfunction, void **trampoline)
{
	enabled = false;
	detoured = false;
	detour_address = NULL;
	detour_trampoline = NULL;
	this->detour_callback = callbackfunction;
	this->trampoline = trampoline;
}

bool CDetour::Init(void *addr)
{
	detour_address = addr;

	if (!CreateDetour())
	{
		enabled = false;
		return enabled;
	}

	enabled = true;

	return enabled;
}

void CDetour::Destroy(bool undoPatch)
{
	if (undoPatch)
		detoured = false;

	DeleteDetour();
	delete this;
}

bool CDetour::IsEnabled()
{
	return enabled;
}

void *CDetour::GetTargetAddr()
{
	return detour_address;
}

bool CDetour::CreateDetour()
{
	if (!detour_address)
	{
		return false;
	}
	
	int requiredSize = OP_JMP_SIZE;
	
#if defined(_WIN64) || defined(__x86_64__)
	if (!IsShortJump(codegen, detour_address))
		requiredSize = X64_ABS_SIZE;
#endif
	
	/*
	 * Determine how many bytes to save from target function.
	 * We want 5 for our detour jmp, but it could require more.
	 */
	detour_restore.bytes = copy_bytes((unsigned char *)detour_address, NULL, requiredSize);
	
	/* First, save restore bits */
	memcpy(detour_restore.patch, (unsigned char *)detour_address, detour_restore.bytes);
	
	/* Patch old bytes in */
	codegen.alloc(detour_restore.bytes);
	copy_bytes((unsigned char *)detour_address, codegen.GetData(), detour_restore.bytes);
	
	/* Return to the original function */
	AbsJump(codegen, (unsigned char *)detour_address + detour_restore.bytes);

	codegen.SetRE();

	*trampoline = codegen.GetData();

	return true;
}

void CDetour::DeleteDetour()
{
	if (detoured)
	{
		DisableDetour();
	}

	if (detour_trampoline)
	{
		/* Free the allocated trampoline memory */
		codegen.clear();
		detour_trampoline = NULL;
	}
}

void CDetour::EnableDetour()
{
	if (!detoured)
	{
		DoGatePatch((unsigned char *)detour_address, detour_callback);
		detoured = true;
	}
}

void CDetour::DisableDetour()
{
	if (detoured)
	{
		/* Remove the patch */
		ApplyPatch(detour_address, 0, &detour_restore, NULL);
		detoured = false;
	}
}
