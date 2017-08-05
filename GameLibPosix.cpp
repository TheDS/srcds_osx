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

// POSIX implementation of GameLib

#include <dlfcn.h>
#include <string.h>
#include "GameLib.h"

#if defined(PLATFORM_MACOSX)
#define LIBEXT ".dylib"
#else
#define LIBEXT ".so"
#endif

GameLib::GameLib() : shortName_(nullptr), handle_(nullptr)
{

}

GameLib::GameLib(const char *name) : handle_(nullptr)
{
    Load(name);
}

GameLib::~GameLib()
{
    Close();
}

const AString& GameLib::GetName() const
{
    return name_;
}

bool GameLib::IsLoaded() const
{
    return handle_ != NULL;
}

bool GameLib::Load(const char *name)
{
    if (IsLoaded())
        Close();
    
    shortName_ = name;
	name_ = name;
    
#if defined(PLATFORM_LINUX)
    // On Linux, look for libraries with _srv suffix first
    name_.append("_srv" LIBEXT);
    
    if (TryLoad())
        return IsLoaded();
    
    name_ = name;
    name_.append(LIBEXT);
    
    if (TryLoad())
        return IsLoaded();

    name_ = "lib";
    name_.append(name);
    name_.append("_srv" LIBEXT);
    
    if (TryLoad())
        return IsLoaded();

    name_ = "lib";
    name_.append(name);
    name_.append(LIBEXT);
#else
    name_.append(LIBEXT);
    
    if (TryLoad())
        return IsLoaded();
    
    name_ = "lib";
    name_.append(name);
    name_.append(LIBEXT);
#endif
    
    return TryLoad();
}

void GameLib::Close()
{
    if (handle_)
    {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

void *GameLib::GetSymbolAddr(const char *symbol)
{
    return dlsym(handle_, symbol);
}

bool GameLib::TryLoad()
{
    handle_ = dlopen(name_.chars(), RTLD_LAZY);
    return IsLoaded();
}

