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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

#include "hacks.h"
#include "mm_util.h"
#include "cocoa_helpers.h"

#define STEAMWORKS_CLIENT_INTERFACES
#include "osw/Steamclient.h"

typedef int (*DedicatedMain_t)(int argc, char **argv);
extern void *g_Launcher;

#if defined(ENGINE_CSGO)
extern void *g_EmptyShader;
#endif

/*
 * Set SteamAppId environment var to appid of dedicated server tool.
 *
 * NOTE: 310 causes an assertion failure in one of the steam libraries, but this
 * happens on Windows too, so it doesn't seem to be a big deal.
 */
void SetSteamAppId()
{
#if defined(ENGINE_OBV)
	setenv("SteamAppId", "310", 1);
#elif defined(ENGINE_L4D)
	setenv("SteamAppId", "510", 1);
#elif defined(ENGINE_L4D2)
	setenv("SteamAppId", "560", 1);
#elif defined(ENGINE_CSGO)
	setenv("SteamAppId", "740", 1);
#else
#error Source engine version not defined!
#endif
}

/* Set SteamAppUser environment var to current Steam user's account name */
bool SetSteamAppUser()
{
	void *steamclient = dlopen("steamclient.dylib", RTLD_LAZY);
	if (!steamclient)
	{
		printf("Failed to open steamclient.dylib!\n");
		return false;
	}
	
	CreateInterfaceFn factory = (CreateInterfaceFn)dlsym(steamclient, "CreateInterface");
	if (!factory)
	{
		printf("Failed get steam client interface factory!\n");
		dlclose(steamclient);
		return false;
	}
	
	IClientEngine *clientEngine = (IClientEngine *)factory(CLIENTENGINE_INTERFACE_VERSION, NULL);
	if (!clientEngine)
	{
		printf("Failed to get steam client engine interface!\n");
		dlclose(steamclient);
		return false;
	}
	
	HSteamPipe pipe = clientEngine->CreateSteamPipe();
	if (!pipe)
	{
		printf("Failed to create pipe to steam client! Is Steam running?\n");
		dlclose(steamclient);
		return false;
	}
	
	HSteamUser user = clientEngine->ConnectToGlobalUser(pipe);
	if (!user)
	{
		printf("Failed to connect to steam client global user!\n");
		clientEngine->BReleaseSteamPipe(pipe);
		dlclose(steamclient);
		return false;
	}

	IClientUser *clientUser = clientEngine->GetIClientUser(user, pipe, CLIENTUSER_INTERFACE_VERSION);
	if (!clientUser)
	{
		printf("Failed to get steam client user interface!\n");
		clientEngine->ReleaseUser(pipe, user);
		clientEngine->BReleaseSteamPipe(pipe);
		dlclose(steamclient);
		return false;
	}

	typedef const char * (*GetAccountName_t)(void *, char *, unsigned int);
	GetAccountName_t GetAccountName = (GetAccountName_t)GetAccountNameFunc((void *)factory);
	
	char account[256];
	if (!GetAccountName(clientUser, account, sizeof(account)) || account[0] == '\0')
	{
		printf("Failed to get account name of current steam user!\n");
		clientEngine->ReleaseUser(pipe, user);
		clientEngine->BReleaseSteamPipe(pipe);
		dlclose(steamclient);
		return false;
	}
	
	/* Needed to mount steam content */
	setenv("SteamAppUser", account, 1);

	clientEngine->ReleaseUser(pipe, user);
	clientEngine->BReleaseSteamPipe(pipe);
	dlclose(steamclient);

	return true;
}

/* Read gameinfo.txt in order to get the appid for game content */
int GetContentAppId(const char *game)
{
	char gameinfo_path[PATH_MAX];
	mm_Format(gameinfo_path, sizeof(gameinfo_path), "%s/gameinfo.txt", game);
	
	FILE *fp = fopen(gameinfo_path, "rt");
	if (!fp)
	{
		printf("Failed to read %s!\n", gameinfo_path);
		return 0;
	}
	
	bool filesys = false;
	char line[256], key[128], value[128];
	long id = 0;
	while (!feof(fp) && fgets(line, sizeof(line), fp) != NULL)
	{
		mm_TrimComments(line);
		mm_TrimLeft(line);
		mm_TrimRight(line);
		
		if (strcasecmp(line, "FileSystem") == 0)
		{
			filesys = true;
		}
		
		if (!filesys)
		{
			continue;
		}
		
		mm_KeySplit(line, key, sizeof(key), value, sizeof(value));
		
		if (strcasecmp(key, "SteamAppId") == 0)
		{
			id = strtol(value, NULL, 10);
			break;
		}
	}
	
	if (id == 0)
	{
		printf("Couldn't find SteamAppId in gameinfo.txt!\n");
	}
	
	fclose(fp);
	
	return id;
}

int main(int argc, char **argv)
{
	bool steam = false;
	char steamPath[PATH_MAX];
	const char *game = NULL;

	for (int i = 1; i < argc; i++)
	{
		if (!game && strcmp(argv[i], "-game") == 0)
		{
			/* We detect this is order to read gameinfo.txt using GetAppId */
			int n = i + 1;
			if (n != argc && argv[n][0] != '-' && argv[n][0] != '+')
			{
				game = argv[n];
			}
		}
#if defined(ENGINE_OBV)
		else if (!steam && strcmp(argv[i], "-steam") == 0)
		{
			/* 
			 * This option is normally passed by the steam client in order to use the steam filesystem,
			 * so we detect this in order to use the correct set of fixes/hacks.
			 */
			if (!GetSteamPath(steamPath, sizeof(steamPath)))
			{
				printf("Couldn't find path to Steam. Is it installed?\n");
				return -1;
			}
			steam = true;
		}
#endif
	}

	/* Get steam path anyways in case it's installed. This will simplify setup for running servers when Steam isn't actually running */
	if (!steam)
	{
		GetSteamPath(steamPath, sizeof(steamPath));
	}

	/* Initialize symbol offsets for various libraries that we will be using */
	if (!InitSymbolData(steam ? steamPath : NULL))
	{
		return -1;
	}

	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd)))
	{
		printf("getcwd failed (%s)\n", strerror(errno));
		return -1;
	}

	char libPath[PATH_MAX];
	char *oldPath = getenv("DYLD_LIBRARY_PATH");
	mm_Format(libPath, sizeof(libPath), "%s:%s:%s/bin:%s/bin/osx32:%s", steamPath, cwd, cwd, cwd, oldPath);
	
	if (SetLibraryPath(libPath) != 0)
	{
		printf("Failed to set library path!\n");
		return -1;
	}
	
	int appid = 0;
	if (steam)
	{
		SetSteamAppId();

		if (!SetSteamAppUser())
		{
			return -1;
		}
		
#if defined(ENGINE_OBV)
		/* Get appid so that we can mount content */
		if (!(appid = GetContentAppId(game)))
		{
			return -1;
		}
#endif
	}

	void *lib = dlopen("dedicated.dylib", RTLD_NOW);
	if (!lib)
	{
		printf("Failed to open dedicated.dylib (%s)\n",  dlerror());
		return -1;
	}

	DedicatedMain_t DedicatedMain = (DedicatedMain_t)dlsym(lib, "DedicatedMain");
	if (!DedicatedMain)
	{
		printf("Failed to find dedicated server entry point (%s)\n", dlerror());
		dlclose(lib);
		return -1;
	}

	if (!DoDedicatedHacks((void *)DedicatedMain, steam, appid))
	{
		dlclose(lib);
		return -1;
	}

#if defined(ENGINE_OBV)
	if (steam)
	{
		char appString[16];
		mm_Format(appString, sizeof(appString), "%d", appid);
		setenv("SteamAppId", appString, 1);
		if (!ForceSteamAppId(appid))
		{
			printf("Warning: Failed to force appid %d - clients may be unable to connect\n", appid);
		}
	}
#endif

	/* 
	 * Prevent problem where files can't be found when executable path contains spaces.
	 * We need to put quotation marks around it if necessary.
	 * It seems this can happen when running under gdb or lldb.
	 */
	char *execPath = NULL;
	if (argv[0][0] != '"' && strstr(argv[0], " "))
	{
		/* Need extra space for quotation marks */
		size_t len = strlen(argv[0]) + 3;
		execPath = new char[len];
		
		/* Copy original path */
		strcpy(&execPath[1], argv[0]);
		
		/* Add quotations marks and null terminate */
		execPath[0] = execPath[len - 2] = '"';
		execPath[len - 1] = '\0';
		
		argv[0] = execPath;
	}
	
#if defined(ENGINE_L4D)
	typedef void (*BuildCmdLine_t)(int argc, char **argv);
	BuildCmdLine_t BuildCmdLine = (BuildCmdLine_t)GetBuildCmdLine();

	if (!BuildCmdLine)
	{
		dlclose(lib);
		delete[] execPath;
		return -1;
	}
	
	/* Game does not parse command line without this */
	BuildCmdLine(argc, argv);
#endif

	InitCocoaPool();
	int result = DedicatedMain(argc, argv);
	ReleaseCocoaPool();

	RemoveDedicatedDetours();

	/* Unload launcher.dylib */
	if (g_Launcher)
	{
		dlclose(g_Launcher);
	}

#if defined(ENGINE_CSGO)
	if (g_EmptyShader)
	{
		dlclose(g_EmptyShader);
	}
#endif

	/* Unload dedicated.dylib */
	dlclose(lib);

	delete[] execPath;

	return result;
}
