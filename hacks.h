/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server Wrapper for Mac OS X
 * Copyright (C) 2011 Scott "DS" Ehlert.  All rights reserved.
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
 */

#ifndef _INCLUDE_SRCDS_OSX_HACKS_H_
#define _INCLUDE_SRCDS_OSX_HACKS_H_

/* Initialize data required to resolve hidden symbols */
bool InitSymbolData(const char *steamPath);

/* Sets DYLD_LIBRARY_PATH */
int SetLibraryPath(const char *path);

/* Do hacks/fixes for dedicated.dylib */
bool DoDedicatedHacks(void *entryPoint, bool steam, int appid);

/* Destroy detours for dedicated.dylib */
void RemoveDedicatedDetours();

/* Get address of IClientUser::GetAccountName in steamclient */
void *GetAccountNameFunc(const void *entryPoint);

#if defined(ENGINE_OBV)

/* Force specified app id to be reported by server */
bool ForceSteamAppId(unsigned int appid);

#endif

#if defined(ENGINE_L4D)

/* Get address of BuildCmdLine in tier0 */
void *GetBuildCmdLine();

#endif

#endif // _INCLUDE_SRCDS_OSX_HACKS_H_
