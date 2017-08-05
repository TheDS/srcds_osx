/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server NG - Game API Library
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

#ifndef _INCLUDE_SRCDS_HSGAMELIB_H_
#define _INCLUDE_SRCDS_HSGAMELIB_H_

#include "GameLib.h"
#include "sm_symtable.h"
#include "am-string.h"
#include <sys/types.h>

#if defined(PLATFORM_LINUX)
#include <elf.h>
#include <link.h>
#if defined(PLATFORM_X64)
typedef Elf64_Sym *RawSymbolTable;
#else
typedef Elf32_Sym *RawSymbolTable;
#endif // defined(PLATFORM_X64)
#elif defined(PLATFORM_MACOSX)
#include <mach-o/nlist.h>
#if defined(PLATFORM_X64)
typedef struct nlist_64 *RawSymbolTable;
#else
typedef struct nlist *RawSymbolTable;
#endif // defined(PLATFORM_X64)
#endif

struct SymbolInfo
{
    const char *name;
    void *address;
};

// GameLib subclass capable of finding symbols hidden via gcc or clangs -fvisibility=hidden option
class HSGameLib : public GameLib
{
public:
    HSGameLib();
    explicit HSGameLib(const char *name);
	~HSGameLib();
    
    bool Load(const char *name);
    bool IsValid() const;
    
    template <typename T>
    T ResolveHiddenSymbol(const char *symbol)
    {
       return reinterpret_cast<T>(GetHiddenSymbolAddr(symbol));
    }
    
    size_t ResolveHiddenSymbols(SymbolInfo *list, const char **names);

	void *FindPattern(const char *pattern, size_t len);
    
    static int SetLibraryPath(const char *path);
private:
    void Initialize();
    void Invalidate();
    uintptr_t GetBaseAddress();
    void *GetHiddenSymbolAddr(const char *symbol);
#if defined(PLATFORM_LINUX)
	static int baseaddr_callback(struct dl_phdr_info *info, size_t size, void *data);
	friend int baseaddr_callback(struct dl_phdr_info *info, size_t size, void *data);
#endif
private:
    SymbolTable table_;
    uintptr_t baseAddress_;
    uint32_t lastPosition_;
    RawSymbolTable symbolTable_;
    const char *stringTable_;
    uint32_t symbolCount_;
    bool valid_;
	void *fileHeader_;
	off_t mapSize_;
	off_t searchSize_;
};

#endif // _INCLUDE_SRCDS_HSGAMELIB_H_
