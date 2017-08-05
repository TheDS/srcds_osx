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

#ifndef _INCLUDE_SRCDS_GAMELIB_H_
#define _INCLUDE_SRCDS_GAMELIB_H_

#include "platform.h"
#include "am-string.h"
#include <stddef.h>

using namespace ke;

#if defined(PLATFORM_WINDOWS)
typedef HMODULE LibHandle;
#else
typedef void * LibHandle;
#endif

typedef void *(*CreateInterfaceFn)(const char *, int *);

// Represents a dynamic library for Source games
class GameLib
{
public:
    GameLib();
    explicit GameLib(const char *name);
    virtual ~GameLib();
public:
    const AString& GetName() const;
    bool IsLoaded() const;
    
    virtual bool Load(const char *name);
    void Close();

    template <typename T>
    T ResolveSymbol(const char *symbol)
    {
        return reinterpret_cast<T>(GetSymbolAddr(symbol));
    }
    
    inline CreateInterfaceFn GetFactory()
    {
        return ResolveSymbol<CreateInterfaceFn>("CreateInterface");
    }

    inline operator bool() const
    {
        return IsLoaded();
    }
private:
    // Disallow copy construction and assignment
    GameLib(const GameLib &other);
    GameLib &operator =(const GameLib &other);

    void *GetSymbolAddr(const char *symbol);
    bool TryLoad();
protected:
    AString name_;
    const char *shortName_;
    LibHandle handle_;
};

#endif // _INCLUDE_SRCDS_GAMELIB_H_
