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

#include "hacks.h"
#include "mm_util.h"
#include "CDetour/detours.h"
#include "osw/SteamTypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <list>

#include <AvailabilityMacros.h>
#include <CoreServices/CoreServices.h>
#include <mach/task.h>
#include <mach-o/nlist.h>

/* Define things from 10.6 SDK for older SDKs */
#ifndef MAC_OS_X_VERSION_10_6
#define TASK_DYLD_INFO 17
struct task_dyld_info
{
	mach_vm_address_t all_image_info_addr;
	mach_vm_size_t all_image_info_size;
};
typedef struct task_dyld_info task_dyld_info_data_t;
#define TASK_DYLD_INFO_COUNT (sizeof(task_dyld_info_data_t) / sizeof(natural_t))
#endif

static struct nlist dyld_syms[3];

#if !defined(ENGINE_DOTA)
static struct nlist dedicated_syms[7];
#endif

#if !defined(ENGINE_INS) && !defined(ENGINE_DOTA)
static struct nlist launcher_syms[5];
static struct nlist engine_syms[3];
#endif

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
static struct nlist fsstdio_syms[7];
#endif

#if defined(ENGINE_L4D) || defined(ENGINE_CSGO) || defined(ENGINE_INS)
static struct nlist material_syms[11];
#endif
#if defined(ENGINE_L4D)
static struct nlist tier0_syms[2];
#endif

void *g_Launcher = NULL;

#if defined(ENGINE_CSGO) || defined(ENGINE_INS)
void *g_EmptyShader = NULL;

static void **g_pShaderAPI;
static void **g_pShaderAPIDX8;
static void **g_pShaderDevice;
static void **g_pShaderDeviceDx8;
static void **g_pShaderDeviceMgr;
static void **g_pShaderDeviceMgrDx8;
static void **g_pShaderShadow;
static void **g_pShaderShadowDx8;
static void **g_pHWConfig;
#endif

CDetour *detSysLoadModules = NULL;
CDetour *detGetAppId = NULL;
CDetour *detSetAppId = NULL;
CDetour *detLoadModule = NULL;
CDetour *detFsLoadModule = NULL;
CDetour *detSetShaderApi = NULL;
CDetour *detDebugString = NULL;
CDetour *detSdlInit = NULL;
CDetour *detSdlShutdown = NULL;
CDetour *detDepotSetup = NULL;
CDetour *detDepotMount = NULL;

struct AppSystemInfo_t
{
	const char *m_pModuleName;
	const char *m_pInterfaceName;
};

typedef bool (*AddSystems_t)(void *, AppSystemInfo_t *);
AddSystems_t AppSysGroup_AddSystems = NULL;

template <typename T>
static inline T SymbolAddr(void *base, struct nlist *syms, size_t idx)
{
	return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(base) + syms[idx].n_value);
}

bool InitSymbolData()
{
	memset(dyld_syms, 0, sizeof(dyld_syms));
	dyld_syms[0].n_un.n_name = (char *)"__ZL18_dyld_set_variablePKcS0_";
	dyld_syms[1].n_un.n_name = (char *)"_dyld_all_image_infos";

	if (nlist("/usr/lib/dyld", dyld_syms) != 0)
	{
		printf("Failed to find symbols for dyld\n");
		return false;
	}

#if !defined(ENGINE_DOTA)
	memset(dedicated_syms, 0, sizeof(dedicated_syms));
	dedicated_syms[0].n_un.n_name = (char *)"__ZN4CSys11LoadModulesEP24CDedicatedAppSystemGroup";
	dedicated_syms[1].n_un.n_name = (char *)"__ZN15CAppSystemGroup10AddSystemsEP15AppSystemInfo_t";
#if defined(ENGINE_L4D2)
	dedicated_syms[2].n_un.n_name = (char *)"__Z14Sys_LoadModulePKc9Sys_Flags";
#else
	dedicated_syms[2].n_un.n_name = (char *)"__Z14Sys_LoadModulePKc";
#endif
#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	dedicated_syms[3].n_un.n_name = (char *)"_g_pFileSystem";
	dedicated_syms[4].n_un.n_name = (char *)"__ZL17g_pBaseFileSystem";
	dedicated_syms[5].n_un.n_name = (char *)"_g_FileSystem_Stdio";
#endif

	if (nlist("bin/dedicated.dylib", dedicated_syms) != 0)
	{
		printf("Failed to find symbols for dedicated.dylib\n");
		return false;
	}

#if !defined(ENGINE_INS)
	memset(launcher_syms, 0, sizeof(launcher_syms));
	launcher_syms[0].n_un.n_name = (char *)"__ZN15CAppSystemGroup9AddSystemEP10IAppSystemPKc";
#if defined(ENGINE_OBV_SDL)
	launcher_syms[1].n_un.n_name = (char *)"__Z12CreateSDLMgrv";
	launcher_syms[2].n_un.n_name = (char *)"__ZN7CSDLMgr4InitEv";
	launcher_syms[3].n_un.n_name = (char *)"__ZN7CSDLMgr8ShutdownEv";
#else
	launcher_syms[1].n_un.n_name = (char *)"_g_CocoaMgr";
#endif
	if (nlist("bin/launcher.dylib", launcher_syms) != 0)
	{
		printf("Failed to find symbols for launcher.dylib\n");
		return false;
	}

	memset(engine_syms, 0, sizeof(engine_syms));
#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD) || defined(ENGINE_L4D2)
	engine_syms[0].n_un.n_name = (char *)"_g_pLauncherMgr";
#else
	engine_syms[0].n_un.n_name = (char *)"_g_pLauncherCocoaMgr";
#endif

	if (nlist("bin/engine.dylib", engine_syms) != 0)
	{
		printf("Failed to find symbols for engine.dylib\n");
		return false;
	}
#endif

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
	memset(fsstdio_syms, 0, sizeof(fsstdio_syms));
	fsstdio_syms[0].n_un.n_name = (char *)"__Z14Sys_LoadModulePKc9Sys_Flags";
#if defined(ENGINE_GMOD)
	fsstdio_syms[1].n_un.n_name = (char *)"__ZN9GameDepot6System5SetupEv";
	fsstdio_syms[2].n_un.n_name = (char *)"__ZN9GameDepot6System7GetListEv";
	fsstdio_syms[3].n_un.n_name = (char *)"__ZN9GameDepot6System4LoadEv";
	fsstdio_syms[4].n_un.n_name = (char *)"__ZN9GameDepot6System5MountERN16IGameDepotSystem11InformationE";
	fsstdio_syms[5].n_un.n_name = (char *)"__Z13FillDepotListRSt4listIN16IGameDepotSystem11InformationESaIS1_EE";
#endif
#if defined(ENGINE_GMOD)
	int notFound = nlist("bin/filesystem_stdio.dylib", fsstdio_syms);
	if (notFound == 1 && fsstdio_syms[4].n_value == 0)
	{
		// Development version has an extra bool parameter
		fsstdio_syms[4].n_un.n_name = (char *)"__ZN9GameDepot6System5MountERN16IGameDepotSystem11InformationEb";
		notFound = nlist("bin/filesystem_stdio.dylib", fsstdio_syms);
	}
	if (notFound != 0)
	{
		printf("Failed to find symbols for filesystem_stdio.dylib\n");
		return false;
	}
#else
	if (nlist("bin/filesystem_stdio.dylib", fsstdio_syms) != 0)
	{
		printf("Warning: Failed to find symbols for filesystem_stdio.dylib\n");
	}
#endif
#endif

#if defined(ENGINE_L4D) || defined(ENGINE_CSGO) || defined(ENGINE_INS)
	memset(material_syms, 0, sizeof(material_syms));
	material_syms[0].n_un.n_name = (char *)"__ZN15CMaterialSystem12SetShaderAPIEPKc";

#if defined(ENGINE_CSGO) || defined(ENGINE_INS)
	material_syms[1].n_un.n_name = (char *)"_g_pShaderAPI";
	material_syms[2].n_un.n_name = (char *)"_g_pShaderAPIDX8";
	material_syms[3].n_un.n_name = (char *)"_g_pShaderDevice";
	material_syms[4].n_un.n_name = (char *)"_g_pShaderDeviceDx8";
	material_syms[5].n_un.n_name = (char *)"_g_pShaderDeviceMgr";
	material_syms[6].n_un.n_name = (char *)"_g_pShaderDeviceMgrDx8";
	material_syms[7].n_un.n_name = (char *)"_g_pShaderShadow";
	material_syms[8].n_un.n_name = (char *)"_g_pShaderShadowDx8";
	material_syms[9].n_un.n_name = (char *)"_g_pHWConfig";
#endif

	if (nlist("bin/materialsystem.dylib", material_syms) != 0)
	{
		printf("Failed to find symbols for materialsystem.dylib\n");
		return false;
	}

#if defined(ENGINE_L4D)
	memset(tier0_syms, 0, sizeof(tier0_syms));
	tier0_syms[0].n_un.n_name = (char *)"__Z12BuildCmdLineiPPc";

	if (nlist("bin/libtier0.dylib", tier0_syms) != 0)
	{
		printf("Failed to find symbols for libtier0.dylib\n");
		return false;
	}
#endif

#endif // ENGINE_L4D || ENGINE_CSGO || ENGINE_INS

#endif // !ENGINE_DOTA

	return true;
}

int SetLibraryPath(const char *path)
{
	typedef void (*SetEnv_t)(const char *, const char *);
	int ret = setenv("DYLD_LIBRARY_PATH", path, 1);
	if (ret != 0)
	{
		return ret;
	}

	SInt32 osx_major, osx_minor;
	Gestalt(gestaltSystemVersionMajor, &osx_major);
	Gestalt(gestaltSystemVersionMinor, &osx_minor);

	if ((osx_major == 10 && osx_minor >= 6) || osx_major > 10)
	{
		task_dyld_info_data_t dyld_info;
		mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
		if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&dyld_info, &count) != KERN_SUCCESS)
		{
			printf("Failed to get dyld task info for current process\n");
			return -1;
		}
		/* Shift dyld address; this can happen with ASLR on Lion (10.7) */
		dyld_syms[0].n_value += int32_t(dyld_info.all_image_info_addr - dyld_syms[1].n_value);
	}

	/* A hacky hack */
	typedef void (*SetEnv_t)(const char *, const char *);
	SetEnv_t DyldSetEnv = SymbolAddr<SetEnv_t>(NULL, dyld_syms, 0);
	DyldSetEnv("DYLD_LIBRARY_PATH", path);

	return 0;
}

#if defined(ENGINE_L4D)

/*  Replace .so extension with .dylib */
static inline const char *FixLibraryExt(const char *pModuleName, char *buffer, size_t maxLength)
{
	size_t origLen = strlen(pModuleName);

	/*
	 * 3 extra chars are needed to do this.
	 *
	 * NOTE: 2nd condition is NOT >= due to null terminator.
	 */
	if (origLen > 3 && maxLength > origLen + 3)
	{
		size_t baseLen = origLen - 3;
		if (strncmp(pModuleName + baseLen, ".so", 3) == 0)
		{
			/* Yes, this should be safe now */
			memcpy(buffer, pModuleName, baseLen);
			strcpy(buffer + baseLen, ".dylib");

			return buffer;
		}
	}

	return pModuleName;
}

#endif // ENGINE_L4D

#if defined(ENGINE_L4D) || defined(ENGINE_CSGO) || defined(ENGINE_INS)

/* void CMaterialSystem::SetShaderAPI(const char *) */
DETOUR_DECL_MEMBER1(CMaterialSystem_SetShaderAPI, void, const char *, pModuleName)
{
#if defined(ENGINE_L4D)
	char module[PATH_MAX];

	pModuleName = FixLibraryExt(pModuleName, module, sizeof(module));

	DETOUR_MEMBER_CALL(CMaterialSystem_SetShaderAPI)(pModuleName);
#elif defined(ENGINE_CSGO) || defined(ENGINE_INS)
	CreateInterfaceFn shaderFactory;

	g_EmptyShader = dlopen(pModuleName, RTLD_NOW);
	if (!g_EmptyShader)
	{
		printf("Failed to load shader API from %s\n", pModuleName);
		detSetShaderApi->Destroy();
		return;
	}

	shaderFactory = (CreateInterfaceFn)dlsym(g_EmptyShader, "CreateInterface");
	if (!shaderFactory)
	{
		printf("Failed to get shader factory from %s\n", pModuleName);
		dlclose(g_EmptyShader);
		g_EmptyShader = NULL;
		detSetShaderApi->Destroy();
		return;
	}

	*g_pShaderAPI = shaderFactory("ShaderApi029", NULL);
	*g_pShaderAPIDX8 = *g_pShaderAPI;
	*g_pShaderDevice = shaderFactory("ShaderDevice001", NULL);
	*g_pShaderDeviceDx8 = *g_pShaderDevice;
	*g_pShaderDeviceMgr = shaderFactory("ShaderDeviceMgr001", NULL);
	*g_pShaderDeviceMgrDx8 = *g_pShaderDeviceMgr;
	*g_pShaderShadow = shaderFactory("ShaderShadow010", NULL);
	*g_pShaderShadowDx8 = *g_pShaderShadow;
	*g_pHWConfig = shaderFactory("MaterialSystemHardwareConfig013", NULL);
#endif

	/* We can get rid of this now */
	detSetShaderApi->Destroy();
}

#endif // ENGINE_L4D || ENGINE_CSGO || ENGINE_INS

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD) || defined(ENGINE_L4D2)
DETOUR_DECL_STATIC2(Sys_FsLoadModule, void *, const char *, pModuleName, int, flags)
{
	if (strstr(pModuleName, "chromehtml"))
		return NULL;
	else
		return DETOUR_STATIC_CALL(Sys_FsLoadModule)(pModuleName, flags);
}
#endif

#if !defined(ENGINE_DOTA)
DETOUR_DECL_STATIC1(Sys_LoadModule, void *, const char *, pModuleName)
{
	/* Avoid NSAutoreleasepool leaks from libcef */
	if (strstr(pModuleName, "chromehtml"))
	{
		return NULL;
	}
#if defined(ENGINE_L4D) || defined(ENGINE_CSGO) || defined(ENGINE_INS)
	void *handle = NULL;

#if defined(ENGINE_L4D)
	char module[PATH_MAX];
	pModuleName = FixLibraryExt(pModuleName, module, sizeof(module));
#endif

#if defined(ENGINE_CSGO)
	/* The matchmaking_ds lib is not shipped, so replace with matchmaking.dylib */
	if (char *libName = strstr(pModuleName, "matchmaking_ds.dylib"))
	{
		strcpy(libName, "matchmaking.dylib");
		return DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);
	}
#endif

	handle = DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);

	/* We need to install a detour in the materialsystem library, ugh */
	if (handle && strstr(pModuleName, "bin/materialsystem.dylib"))
	{
		Dl_info info;
		void *materialFactory = dlsym(handle, "CreateInterface");
		void *setShaderApi = NULL;

		if (!materialFactory)
		{
			printf("Failed to find CreateInterface (%s)\n", dlerror());
			return NULL;
		}

		if (!dladdr(materialFactory, &info) || !info.dli_fbase || !info.dli_fname)
		{
			printf("Failed to get base address of materialsystem.dylib\n");
			dlclose(handle);
			return NULL;
		}

		setShaderApi = SymbolAddr<void *>(info.dli_fbase, material_syms, 0);

#if defined(ENGINE_CSGO) || defined(ENGINE_INS)
		g_pShaderAPI = SymbolAddr<void **>(info.dli_fbase, material_syms, 1);
		g_pShaderAPIDX8 = SymbolAddr<void **>(info.dli_fbase, material_syms, 2);
		g_pShaderDevice = SymbolAddr<void **>(info.dli_fbase, material_syms, 3);
		g_pShaderDeviceDx8 = SymbolAddr<void **>(info.dli_fbase, material_syms, 4);
		g_pShaderDeviceMgr = SymbolAddr<void **>(info.dli_fbase, material_syms, 5);
		g_pShaderDeviceMgrDx8 = SymbolAddr<void **>(info.dli_fbase, material_syms, 6);
		g_pShaderShadow = SymbolAddr<void **>(info.dli_fbase, material_syms, 7);
		g_pShaderShadowDx8 = SymbolAddr<void **>(info.dli_fbase, material_syms, 8);
		g_pHWConfig = SymbolAddr<void **>(info.dli_fbase, material_syms, 9);
#endif

		detSetShaderApi = DETOUR_CREATE_MEMBER(CMaterialSystem_SetShaderAPI, setShaderApi);
		if (!detSetShaderApi)
		{
			printf("Failed to create detour for CMaterialSystem::SetShaderAPI\n");
			return NULL;
		}

		detSetShaderApi->EnableDetour();

		return handle;
	}

#endif
	return DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);
}

#endif // !ENGINE_DOTA

DETOUR_DECL_STATIC1(Plat_DebugString, void, const char *, str)
{
	/* Do nothing. K? */
}

#if defined(ENGINE_OBV_SDL)
DETOUR_DECL_MEMBER0(CSDLMgr_Init, int)
{
	/* Prevent SDL from initializing to avoid invoking OpenGL */
	return 1;
}

DETOUR_DECL_MEMBER0(CSDLMgr_Shutdown, void)
{
}
#endif

#if defined(ENGINE_GMOD)

struct GameDepotInfo
{
	int unknown;            // Appears to always be 14
	int depotid;            // Game App ID
	const char *title;      // Game Name
	const char *folder;     // Game Directory
	bool mounted;           // Determines if game content is mounted
	bool vpk;               // Seems to flag games with VPK files before Steam3 became prevalent
	bool owned;             // Determines if game is owned
	bool installed;         // Determines if game is installed
};

typedef std::list<GameDepotInfo> &(*DepotGetList)(void *);
typedef void (*DepotLoad)(void *);
typedef void (*FillDepotList)(std::list<GameDepotInfo> &);

DepotGetList g_GetDepotList;
DepotLoad g_LoadDepots;
FillDepotList g_FillDepotList;

DETOUR_DECL_MEMBER0(GameDepotSys_Setup, void)
{
	static bool initialized = false;

	if (!initialized)
	{
		initialized = true;

		/* Fill depot list with supported games */
		std::list<GameDepotInfo> &depots = g_GetDepotList(this);
		g_FillDepotList(depots);

		for (std::list<GameDepotInfo>::iterator i = depots.begin(); i != depots.end(); i++)
		{
			/* Dedicated servers on Linux and Windows set these for all games */
			i->owned = 1;
			i->installed = 1;
		}

		g_LoadDepots(this);
	}
}

DETOUR_DECL_MEMBER2(GameDepotSys_Mount, bool, GameDepotInfo &, info, bool, unknown)
{
	return true;
}
#endif

#if !defined(ENGINE_INS) && !defined(ENGINE_DOTA)
/* int CSys::LoadModules(CDedicatedAppSystemGroup *) */
DETOUR_DECL_MEMBER1(CSys_LoadModules, int, void *, appsys)
{
	Dl_info info;
	void *launcherMain;
	void *pCocoaMgr;
	void *engine;
	void *engineFactory;
	void **engineCocoa;
	int ret;

	typedef void (*AddSystem_t)(void *, void *, const char *);
	AddSystem_t AppSysGroup_AddSystem;

	g_Launcher = dlopen("launcher.dylib", RTLD_NOW);
	if (!g_Launcher)
	{
		printf("Failed to open launcher.dylib (%s)\n",  dlerror());
		return false;
	}

	launcherMain = dlsym(g_Launcher, "LauncherMain");
	if (!launcherMain)
	{
		printf("Failed to find launcher entry point (%s)\n", dlerror());
		dlclose(g_Launcher);
		g_Launcher = NULL;
		return false;
	}

	memset(&info, 0, sizeof(Dl_info));
	if (!dladdr(launcherMain, &info) || !info.dli_fbase || !info.dli_fname)
	{
		printf("Failed to get base address of launcher.dylib\n");
		dlclose(g_Launcher);
		g_Launcher = NULL;
		return false;
	}

	AppSysGroup_AddSystem = SymbolAddr<AddSystem_t>(info.dli_fbase, launcher_syms, 0);

#if defined(ENGINE_OBV_SDL)
	typedef void *(*CreateSdlMgr)(void);
	CreateSdlMgr createSdlObj = SymbolAddr<CreateSdlMgr>(info.dli_fbase, launcher_syms, 1);
	void *sdlInit = SymbolAddr<void *>(info.dli_fbase, launcher_syms, 2);
	void *sdlShutdown = SymbolAddr<void *>(info.dli_fbase, launcher_syms, 3);

	/* Detour SDL init and shutdown to avoid OpenGL */

	detSdlInit = DETOUR_CREATE_MEMBER(CSDLMgr_Init, sdlInit);
	if (!detSdlInit)
	{
		printf("Failed to create detour for CSDLMgr::Init!\n");
		return false;
	}

	detSdlShutdown = DETOUR_CREATE_MEMBER(CSDLMgr_Shutdown, sdlShutdown);
	if (!detSdlShutdown)
	{
		printf("Failed to create detour for CSDLMgr::Shutdown!\n");
		return false;
	}

	detSdlInit->EnableDetour();
	detSdlShutdown->EnableDetour();

	pCocoaMgr = createSdlObj();
#else
	pCocoaMgr = SymbolAddr<void *>(info.dli_fbase, launcher_syms, 1);
#endif

	/* The engine and material system expect this interface to be available */
#if defined(ENGINE_OBV_SDL)
	AppSysGroup_AddSystem(appsys, pCocoaMgr, "SDLMgrInterface001");
#else
	AppSysGroup_AddSystem(appsys, pCocoaMgr, "CocoaMgrInterface006");
#endif

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
	/* Preload filesystem_stdio to install a detour */
	void *loadModule = NULL;
	void *fs = dlopen("bin/filesystem_stdio.dylib", RTLD_NOW);

	if (fs)
	{
		void *factory = dlsym(fs, "CreateInterface");
		memset(&info, 0, sizeof(Dl_info));
		if (dladdr(factory, &info) && info.dli_fbase && info.dli_fname)
		{
			loadModule = SymbolAddr<void *>(info.dli_fbase, fsstdio_syms, 0);
			if (loadModule != info.dli_fbase)
			{
				detFsLoadModule = DETOUR_CREATE_STATIC(Sys_FsLoadModule, loadModule);
				if (detFsLoadModule)
				{
					detFsLoadModule->EnableDetour();
				}
			}

#if defined(ENGINE_GMOD)
			void *depotSetup, *depotMount;
			depotSetup = SymbolAddr<void *>(info.dli_fbase, fsstdio_syms, 1);
			g_GetDepotList = SymbolAddr<DepotGetList>(info.dli_fbase, fsstdio_syms, 2);
			g_LoadDepots = SymbolAddr<DepotLoad>(info.dli_fbase, fsstdio_syms, 3);
			depotMount = SymbolAddr<void *>(info.dli_fbase, fsstdio_syms, 4);
			g_FillDepotList = SymbolAddr<FillDepotList>(info.dli_fbase, fsstdio_syms, 5);

			detDepotSetup = DETOUR_CREATE_MEMBER(GameDepotSys_Setup, depotSetup);

			if (detDepotSetup)
			{
				detDepotSetup->EnableDetour();
			}
			else
			{
				printf("Failed to create detour for GameDepot::System::Setup!\n");
				dlclose(fs);
				return false;
			}

			detDepotMount = DETOUR_CREATE_MEMBER(GameDepotSys_Mount, depotMount);

			if (detDepotMount)
			{
				detDepotMount->EnableDetour();
			}
			else
			{
				printf("Failed to create detour for GameDepot::System::Mount!\n");
				dlclose(fs);
				return false;
			}
#endif
		}
	}

	if (!detFsLoadModule)
	{
		printf("Warning: Unable to detour Sys_LoadModule in filesystem_stdio\n");
	}
#endif

#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	AppSystemInfo_t sys_before[] =
	{
		{"inputsystem.dylib",	"InputSystemVersion001"},
#if defined(ENGINE_CSGO)
		{"soundemittersystem.dylib",	"VSoundEmitter003"},
#endif
		{"",					""}
	};
	AppSysGroup_AddSystems(appsys, sys_before);
#endif

	/* Call the original */
	ret = DETOUR_MEMBER_CALL(CSys_LoadModules)(appsys);

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
	/* CSys::LoadModules has incremented the filesystem_stdio reference count, so we can close ours */
	if (fs)
		dlclose(fs);
#endif

	/* Engine should already be loaded at this point by the original function */
	engine = dlopen("engine.dylib", RTLD_NOLOAD);
	if (!engine)
	{
		printf("Failed to get existing handle for engine.dylib (%s)\n", dlerror());
		return false;
	}

	engineFactory = dlsym(engine, "CreateInterface");
	if (!engineFactory)
	{
		printf("Failed to find CreateInterface (%s)\n", dlerror());
		dlclose(engine);
		return false;
	}

	if (!dladdr(engineFactory, &info) || !info.dli_fbase || !info.dli_fname)
	{
		printf("Failed to get base address of engine.dylib\n");
		dlclose(engine);
		return false;
	}

	engineCocoa = SymbolAddr<void **>(info.dli_fbase, engine_syms, 0);

	/* Prevent crash in engine function which expects this interface */
	*engineCocoa = pCocoaMgr;

#if defined(ENGINE_PORTAL2)
	/* Horrible fix for crash on exit */
	unsigned char *quit = SymbolAddr<unsigned char *>(info.dli_fbase, engine_syms, 1);
	SetMemPatchable(quit, 32);
	quit += 12;
	quit[0] = 0xEB;
	quit[1] = 0x22;
#endif

#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	/* Load these to prevent crashes in engine and replay system */
	AppSystemInfo_t sys_after[] =
	{
#if defined(ENGINE_ND) || defined(ENGINE_L4D2) || defined(ENGINE_CSGO)
		{"vguimatsurface.dylib",	"VGUI_Surface031"},
#else
		{"vguimatsurface.dylib",	"VGUI_Surface030"},
#endif
		{"vgui2.dylib",				"VGUI_ivgui008"},
		{"",						""}
	};
	AppSysGroup_AddSystems(appsys, sys_after);
#endif

	dlclose(engine);

	return ret;
}
#endif // !ENGINE_INS && !ENGINE_DOTA

bool DoDedicatedHacks(void *entryPoint)
{
	void *tier0, *dbgString;
#if !defined(ENGINE_DOTA)
	Dl_info info;
	void *sysLoad, *loadModule;
#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	void **pFileSystem;
	void **pBaseFileSystem;
	void *fileSystem;
#endif

	memset(&info, 0, sizeof(Dl_info));
	if (!dladdr(entryPoint, &info) || !info.dli_fbase || !info.dli_fname)
	{
		printf("Failed to get base address of dedicated.dylib\n");
		return false;
	}

	sysLoad = SymbolAddr<unsigned char *>(info.dli_fbase, dedicated_syms, 0);
	AppSysGroup_AddSystems = SymbolAddr<AddSystems_t>(info.dli_fbase, dedicated_syms, 1);
	loadModule = SymbolAddr<void *>(info.dli_fbase, dedicated_syms, 2);
#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	pFileSystem = SymbolAddr<void **>(info.dli_fbase, dedicated_syms, 3);
	pBaseFileSystem = SymbolAddr<void **>(info.dli_fbase, dedicated_syms, 4);
	fileSystem = SymbolAddr<void *>(info.dli_fbase, dedicated_syms, 5);

	/* Work around conflicts between FileSystem_Stdio and FileSystem_Steam */
	*pFileSystem = fileSystem;
	*pBaseFileSystem = fileSystem;
#endif

#if !defined(ENGINE_INS)
	/* Detour CSys::LoadModules() */
	detSysLoadModules = DETOUR_CREATE_MEMBER(CSys_LoadModules, sysLoad);
	if (!detSysLoadModules)
	{
		printf("Failed to create detour for CSys::LoadModules\n");
		return false;
	}
#endif

#if defined(ENGINE_GMOD) || defined(ENGINE_L4D2)
	detLoadModule = DETOUR_CREATE_STATIC(Sys_FsLoadModule, loadModule);
#else
	detLoadModule = DETOUR_CREATE_STATIC(Sys_LoadModule, loadModule);
#endif
	if (!detLoadModule)
	{
		printf("Failed to create detour for Sys_LoadModule\n");
		return false;
	}

	detLoadModule->EnableDetour();

#if !defined(ENGINE_INS)
	detSysLoadModules->EnableDetour();
#endif

#endif // !defined(ENGINE_DOTA)

	/* Failing to find Plat_DebugString is non-fatal */
	tier0 = dlopen("libtier0.dylib", RTLD_NOLOAD);
	if (tier0)
	{
		dbgString = dlsym(tier0, "Plat_DebugString");
		detDebugString = DETOUR_CREATE_STATIC(Plat_DebugString, dbgString);

		if (detDebugString)
		{
			detDebugString->EnableDetour();
		}

		dlclose(tier0);
	}

	return true;
}

void RemoveDedicatedDetours()
{
	if (detDebugString)
	{
		detDebugString->Destroy();
	}

#if !defined(ENGINE_DOTA)

#if !defined(ENGINE_INS)
	if (detSysLoadModules)
	{
		detSysLoadModules->Destroy();
	}
#endif

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
	if (detFsLoadModule)
	{
		detFsLoadModule->Destroy();
	}
#endif

	if (detLoadModule)
	{
		detLoadModule->Destroy();
	}

#if defined(ENGINE_OBV_SDL)
	if (detSdlInit)
	{
		detSdlInit->Destroy();
	}

	if (detSdlShutdown)
	{
		detSdlShutdown->Destroy();
	}
#endif

#if defined(ENGINE_GMOD)
	if (detDepotSetup)
	{
		detDepotSetup->Destroy();
	}

	if (detDepotMount)
	{
		detDepotMount->Destroy();
	}
#endif

#endif // !defined(ENGINE_DOTA)
}

#if defined(ENGINE_L4D)
void *GetBuildCmdLine()
{
	Dl_info info;
	void *tier0 = NULL;
	void *msg = NULL;

	tier0 = dlopen("libtier0.dylib", RTLD_NOLOAD);
	if (!tier0)
	{
		printf("Failed to get existing handle for libtier0.dylib (%s)\n", dlerror());
		return NULL;
	}

	msg = dlsym(tier0, "Msg");
	if (!msg)
	{
		printf("Failed to find Msg (%s)\n", dlerror());
		dlclose(tier0);
		return NULL;
	}

	if (!dladdr(msg, &info) || !info.dli_fbase || !info.dli_fname)
	{
		printf("Failed to get base address of libtier0.dylib\n");
		dlclose(tier0);
		return NULL;
	}

	dlclose(tier0);

	return SymbolAddr<void *>(info.dli_fbase, tier0_syms, 0);
}
#endif
