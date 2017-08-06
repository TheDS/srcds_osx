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
#include <crt_externs.h>

#include "platform.h"
#include "HSGameLib.h"

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

#if defined(PLATFORM_X86)
static struct nlist dyld_syms[3];
#endif

#if !defined(ENGINE_DOTA) && !defined(ENGINE_CSGO)
static struct nlist dedicated_syms[7];
#endif

#if !defined(ENGINE_INS) && !defined(ENGINE_DOTA) && !defined(ENGINE_CSGO)
static struct nlist launcher_syms[5];
static struct nlist engine_syms[3];
#endif

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
static struct nlist fsstdio_syms[7];
#endif

#if defined(ENGINE_L4D) || defined(ENGINE_INS)
static struct nlist material_syms[11];
#endif
#if defined(ENGINE_L4D)
static struct nlist tier0_syms[2];
#endif

static struct nlist steamclient_syms[2];

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
CDetour *detSteamLoadModule = NULL;
CDetour *detSetShaderApi = NULL;
CDetour *detDebugString = NULL;
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

#if defined(PLATFORM_X64) && defined(ENGINE_CSGO)
static SymbolInfo dedicated_syms_[3];

static inline void dumpUnknownSymbols(const SymbolInfo *info, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		if (!info[i].address && info[i].name)
			puts(info[i].name);
	}
}

bool InitSymbolData(const char *steamPath)
{
	HSGameLib dedicated("dedicated");
	const char *symbols[] = {
		"_ZN4CSys11LoadModulesEP24CDedicatedAppSystemGroup",
		"_ZN15CAppSystemGroup10FindSystemEPKc",
		"_Z14Sys_LoadModulePKc",
		nullptr
	};
	
	int notFound = dedicated.ResolveHiddenSymbols(dedicated_syms_, symbols);
	if (notFound > 0)
	{
		printf("Failed to find symbols for dedicated.dylib\n");
		dumpUnknownSymbols(dedicated_syms_, ARRAY_LENGTH(dedicated_syms_));
		return false;
	}

	return true;
}

int SetLibraryPath(const char *path)
{
	return HSGameLib::SetLibraryPath(path);
}

#else

static inline void dumpUnknownSymbols(struct nlist *syms)
{
		while (syms->n_un.n_name)
		{
				if (syms->n_value == 0)
						puts(syms->n_un.n_name);
				syms++;
		}
}

bool InitSymbolData(const char *steamPath)
{
	memset(dyld_syms, 0, sizeof(dyld_syms));
	dyld_syms[0].n_un.n_name = (char *)"__ZL18_dyld_set_variablePKcS0_";
	dyld_syms[1].n_un.n_name = (char *)"_dyld_all_image_infos";

	if (nlist("/usr/lib/dyld", dyld_syms) != 0)
	{
		printf("Failed to find symbols for dyld\n");
		dumpUnknownSymbols(dyld_syms);
		return false;
	}

#if !defined(ENGINE_DOTA)
	memset(dedicated_syms, 0, sizeof(dedicated_syms));
	dedicated_syms[0].n_un.n_name = (char *)"__ZN4CSys11LoadModulesEP24CDedicatedAppSystemGroup";
	dedicated_syms[1].n_un.n_name = (char *)"__ZN15CAppSystemGroup10AddSystemsEP15AppSystemInfo_t";
#if defined(ENGINE_L4D2) || defined(ENGINE_ND) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD)
	dedicated_syms[2].n_un.n_name = (char *)"__Z14Sys_LoadModulePKc9Sys_Flags";
#else
	dedicated_syms[2].n_un.n_name = (char *)"__Z14Sys_LoadModulePKc";
#endif
#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	dedicated_syms[3].n_un.n_name = (char *)"_g_pFileSystem";
	dedicated_syms[4].n_un.n_name = (char *)"__ZL17g_pBaseFileSystem";
	dedicated_syms[5].n_un.n_name = (char *)"_g_FileSystem_Stdio";
#endif
#if defined(ENGINE_L4D)
	dedicated_syms[6].n_un.n_name = (char *)"_g_szEXEName";
#endif

	if (nlist("bin/dedicated.dylib", dedicated_syms) != 0)
	{
		printf("Failed to find symbols for dedicated.dylib\n");
		dumpUnknownSymbols(dedicated_syms);
		return false;
	}

#if !defined(ENGINE_INS)
	memset(launcher_syms, 0, sizeof(launcher_syms));
	launcher_syms[0].n_un.n_name = (char *)"__ZN15CAppSystemGroup9AddSystemEP10IAppSystemPKc";
#if !defined(ENGINE_OBV_SDL)
	launcher_syms[1].n_un.n_name = (char *)"_g_CocoaMgr";
#endif
	if (nlist("bin/launcher.dylib", launcher_syms) != 0)
	{
		printf("Failed to find symbols for launcher.dylib\n");
		dumpUnknownSymbols(launcher_syms);
		return false;
	}

	memset(engine_syms, 0, sizeof(engine_syms));
#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD) || defined(ENGINE_L4D2) || defined(ENGINE_ND) || defined(ENGINE_CSGO)
	engine_syms[0].n_un.n_name = (char *)"_g_pLauncherMgr";
#else
	engine_syms[0].n_un.n_name = (char *)"_g_pLauncherCocoaMgr";
#endif

	if (nlist("bin/engine.dylib", engine_syms) != 0)
	{
		printf("Failed to find symbols for engine.dylib\n");
		dumpUnknownSymbols(engine_syms);
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
		dumpUnknownSymbols(fsstdio_syms);
		return false;
	}
#else
	if (nlist("bin/filesystem_stdio.dylib", fsstdio_syms) != 0)
	{
		printf("Warning: Failed to find symbols for filesystem_stdio.dylib\n");
		dumpUnknownSymbols(fsstdio_syms);
	}
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
		dumpUnknownSymbols(material_syms);
		return false;
	}

#if defined(ENGINE_L4D)
	memset(tier0_syms, 0, sizeof(tier0_syms));
	tier0_syms[0].n_un.n_name = (char *)"__ZL12linuxCmdline";

	if (nlist("bin/libtier0.dylib", tier0_syms) != 0)
	{
		printf("Failed to find symbols for libtier0.dylib\n");
		dumpUnknownSymbols(tier0_syms);
		return false;
	}
#endif

#endif // ENGINE_L4D || ENGINE_CSGO || ENGINE_INS

	memset(steamclient_syms, 0, sizeof(steamclient_syms));
	steamclient_syms[0].n_un.n_name = (char *)"__Z14Sys_LoadModulePKc9Sys_Flags";
	
	if (steamPath)
	{
		char clientPath[PATH_MAX];
		mm_Format(clientPath, sizeof(clientPath), "%s/steamclient.dylib", steamPath);

		int ret = nlist(clientPath, steamclient_syms);
		if (ret == -1)
			ret = nlist("bin/steamclient.dylib", steamclient_syms);
			
		if (ret != 0)
		{
			printf("Failed to find symbols for steamclient.dylib\n");
			dumpUnknownSymbols(steamclient_syms);
			return false;
		}
	}

#endif // !ENGINE_DOTA
	return true;
#endif
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
#endif // defined(PLATFORM_X64) && defined(ENGINE_CSGO)

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

#if defined(ENGINE_OBV) || defined(ENGINE_OBV_SDL) || defined(ENGINE_GMOD) || defined(ENGINE_L4D2) || defined(ENGINE_ND)
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
	if (handle && strstr(pModuleName, "materialsystem.dylib"))
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
		
#if defined(ENGINE_CSGO)
		HSGameLib matsys("materialsystem");
		
		/*const char *symbols[] = {
			"_ZN15CMaterialSystem12SetShaderAPIEPKc",
			"g_pShaderAPI",
			"g_pShaderAPIDX8",
			"g_pShaderDevice",
			"g_pShaderDeviceDx8",
			"g_pShaderDeviceMgr",
			"g_pShaderDeviceMgrDx8",
			"g_pShaderShadow",
			"g_pShaderShadowDx8",
			"g_pHWConfig",
			nullptr
		};
	
		int notFound = matsys.ResolveHiddenSymbols(material_syms_, symbols);
		if (notFound > 0)
		{
			printf("Failed to find symbols for materialsystem.dylib\n");
			dumpUnknownSymbols(material_syms_, ARRAY_LENGTH(material_syms_));
			return NULL;
		}

		setShaderApi = material_syms_[0].address;

#if defined(ENGINE_CSGO) || defined(ENGINE_INS)
		g_pShaderAPI = (void **)material_syms_[1].address;
		g_pShaderAPIDX8 = (void **)material_syms_[2].address;
		g_pShaderDevice = (void **)material_syms_[3].address;
		g_pShaderDeviceDx8 = (void **)material_syms_[4].address;
		g_pShaderDeviceMgr = (void **)material_syms_[5].address;
		g_pShaderDeviceMgrDx8 = (void **)material_syms_[6].address;
		g_pShaderShadow = (void **)material_syms_[7].address;
		g_pShaderShadowDx8 = (void **)material_syms_[8].address;
		g_pHWConfig = (void **)material_syms_[9].address;
#endif */

		void ***vptr = (void ***)matsys.GetFactory()("VMaterialSystem080", NULL);
		void **vtable = *vptr;
		setShaderApi = vtable[10]; // IMaterialSystem::SetShaderAPI

		detSetShaderApi = DETOUR_CREATE_MEMBER(CMaterialSystem_SetShaderAPI, setShaderApi);
		if (!detSetShaderApi)
		{
			printf("Failed to create detour for CMaterialSystem::SetShaderAPI\n");
			return NULL;
		}

		detSetShaderApi->EnableDetour();

		// g_pShaderAPI: CShaderDeviceBase::GetWindowSize
		{
			const char sig[] = "\x55\x48\x89\xE5\x48\x8B\x3D\x2A\x2A\x2A\x2A\x48\x8B\x07\x48\x8B\x80\xA8\x00\x00\x00";
			const int offset = 7;		
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderAPI\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderAPI = (void **)(p + offset + 4 + shaderOffs);
		}
		
		// g_pShaderAPIDX8: CMatRenderContext::SetLights
		{
			const char sig[] = "\x55\x48\x89\xE5\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\x48\x8B\x80\x38\x03\x00\x00";
			const int offset = 7;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderAPIDX8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderAPIDX8 = (void **)(p + offset + 4 + shaderOffs);
		}
		
		// g_pShaderDevice: CMatRenderContext::DestoryStaticMesh
		{
			const char sig[] = "\x55\x48\x89\xE5\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\x48\x8B\x80\xC0\x00\x00\x00";
			const int offset = 7;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderDevice\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDevice = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderDeviceDx8: CDynamicMeshDX8::HasEnoughRoom
		{
			const char sig[] = "\x55\x48\x89\xE5\x41\x57\x41\x56\x53\x50\x41\x89\xD6\x89\xF3\x49\x89\xFF\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\xFF\x90\x30\x01\x00\x00";
			const int offset = 21;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderDeviceDx8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDeviceDx8 = (void **)(p + offset + 4 + shaderOffs);
		}

		// g_pShaderDeviceMgr: CMaterialSystem::GetModeCount
		{
			const char sig[] = "\x55\x48\x89\xE5\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\x48\x8B\x40\x60";
			const int offset = 7;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderDeviceMgr\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDeviceMgr = (void **)(p + offset + 4 + shaderOffs);
		}
		
		// g_pShaderDeviceMgrDx8: CShaderAPIDx8::OnDeviceInit
		{
			const char sig[] = "\x55\x48\x89\xE5\x41\x57\x41\x56\x53\x50\x48\x89\xFB\xE8\x2A\x2A\x2A\x2A\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\x8B\x73\x08";
			const int offset = 21;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderDeviceMgrDx8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderDeviceMgrDx8 = (void **)(p + offset + 4 + shaderOffs);
		}
		
		// g_pShaderShadow: CShaderSystem::TakeSnapshot
		{
			const char sig[] = "\x55\x48\x89\xE5\x41\x57\x41\x56\x53\x50\x49\x89\xFF\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\xFF\x90\x88\x00\x00\x00\x83\xF8\x5C\x7C\x33\x4C\x8D\x35\x2A\x2A\x2A\x2A\x49";
			const int offset = 40;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderShadow\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderShadow = (void **)(p + offset + 4 + shaderOffs);
		}
		
		// g_pShaderShadowDx8: CShaderAPIDx8::ClearSnapshots
		{
			const char sig[] = "\x55\x48\x89\xE5\x41\x56\x53\x48\x89\xFB\x4C\x8D\xB3\x78\x34\x00\x00\x4C\x89\xF7\xE8\x2A\x2A\x2A\x2A\x48\x8D\x05\x2A\x2A\x2A\x2A\x48";
			const int offset = 28;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pShaderShadowDx8\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pShaderShadowDx8 = (void **)(p + offset + 4 + shaderOffs);
		}
		
		// g_pHWConfig: CMaterialSystem::SupportsHDRMode
		{
			const char sig[] = "\x55\x48\x89\xE5\x48\x8B\x3D\x2A\x2A\x2A\x2A\x48\x8B\x07\x48\x8B\x80\x68\x01\x00\x00";
			const int offset = 7;
			char *p = (char *)matsys.FindPattern(sig, sizeof(sig) - 1);
			if (!p)
			{
				printf("Failed to find signature to locate g_pHWConfig\n");
				return nullptr;
			}
			uint32_t shaderOffs = *(uint32_t *)(p + offset);
			g_pHWConfig = (void **)(p + offset + 4 + shaderOffs);
		}

		return handle;
#else
		setShaderApi = SymbolAddr<void *>(info.dli_fbase, material_syms, 0);

#if defined(ENGINE_INS)
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
#endif
	}

#endif

	return DETOUR_STATIC_CALL(Sys_LoadModule)(pModuleName);
}

#endif // !ENGINE_DOTA

DETOUR_DECL_STATIC1(Plat_DebugString, void, const char *, str)
{
	/* Do nothing. K? */
}

DETOUR_DECL_STATIC2(Sys_SteamLoadModule, void *, const char *, pModuleName, int, flags)
{
	if (strstr(pModuleName, "steamservice"))
		return NULL;
	else
		return DETOUR_STATIC_CALL(Sys_SteamLoadModule)(pModuleName, flags);
}

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

bool BlockSteamService()
{
	void *steamclient = dlopen("steamclient.dylib", RTLD_LAZY);
	if (!steamclient)
	{
		printf("Failed to get handle for steamclient.dylib (%s)\n", dlerror());
		return false;
	}
	
	void *steamclientFactory = dlsym(steamclient, "CreateInterface");
	if (!steamclientFactory)
	{
		printf("Failed to find steamclient CreateInterface (%s)\n", dlerror());
		dlclose(steamclient);
		return false;
	}
	
	Dl_info info;
	if (!dladdr(steamclientFactory, &info) || !info.dli_fbase || !info.dli_fname)
	{
		printf("Failed to get base address of steamclient.dylib\n");
		dlclose(steamclient);
		return false;
	}
	
	void *steamLoadModule = SymbolAddr<void *>(info.dli_fbase, steamclient_syms, 0);
	detSteamLoadModule = DETOUR_CREATE_STATIC(Sys_SteamLoadModule, steamLoadModule);
	
	if (detSteamLoadModule)
	{
		detSteamLoadModule->EnableDetour();
	}
	else
	{
		printf("Failed to create detour for steamclient`Sys_LoadModule!\n");
		dlclose(steamclient);
		return false;
	}
	
	dlclose(steamclient);
	return true;
}

#if defined(ENGINE_CSGO)
DETOUR_DECL_MEMBER1(CSys_LoadModules, int, void *, appsys)
{
	/* Preload filesystem_stdio to install a detour */
	void *loadModule = NULL;
	HSGameLib fs("bin/osx64/filesystem_stdio");

	if (fs.IsLoaded())
	{
		//loadModule = fs.ResolveHiddenSymbol("_Z14Sys_LoadModulePKc");
		const char sig[] = "\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x54\x53\x48\x81\xEC\x10\x08\x00\x00";
		loadModule = fs.FindPattern(sig, sizeof(sig) - 1);
		if (!loadModule)
		{
			printf("Failed to find signature for filesystem_stdio.dylib\n");
			printf("_Z14Sys_LoadModulePKc");
			return 0;
		}
		if (loadModule)
		{
			detFsLoadModule = DETOUR_CREATE_STATIC(Sys_LoadModule, loadModule);
			if (detFsLoadModule)
			{
				detFsLoadModule->EnableDetour();
			}
			else
			{
				printf("Failed to create detour for filesystem_stdio`Sys_LoadModule!\n");
				return 0;
			}
		}
	}
	else
	{
		printf("Failed to preload filesystem_stdio\n");
		return 0;
	}
	
	int ret = DETOUR_MEMBER_CALL(CSys_LoadModules)(appsys);
	
	typedef void *(*FindSystem_t)(void *, const char *);
	FindSystem_t AppSysGroup_FindSystem = (FindSystem_t)dedicated_syms_[1].address;
	void *sdl = AppSysGroup_FindSystem(appsys, "SDLMgrInterface001");
	
	HSGameLib engine("engine");
	
	if (!engine.IsLoaded())
	{
		printf("Failed to load existing copy of engine.dylib\n");
		return 0;
	}
	
	// CGame::GetMainWindowAddress
	const char sig[] = "\x55\x48\x89\xE5\x53\x50\x48\x89\xFB\x48\x8D\x05\x2A\x2A\x2A\x2A\x48\x8B\x38\x48\x8B\x07\xFF\x90\x08\x01\x00\x00";
	const int offset = 12;
	//void **engineSdl = engine.ResolveHiddenSymbol<void **>("g_pLauncherMgr");
	char *p = (char *)engine.FindPattern(sig, sizeof(sig) - 1);
	if (!p)
	{
		printf("Failed to find signature for engine.dylib\n");
		printf("g_pLauncherMgr");
		return 0;
	}
	
	uint32_t launcherOffs = *(uint32_t *)(p + offset);
	void **engineSdl = (void **)(p + offset + 4 + launcherOffs);
	
	*engineSdl = sdl;

	return ret;
}

#else

#if !defined(ENGINE_INS) && !defined(ENGINE_DOTA)
/* int CSys::LoadModules(CDedicatedAppSystemGroup *) */
DETOUR_DECL_MEMBER1(CSys_LoadModules, int, void *, appsys)
{
#if !defined(ENGINE_OBV_SDL)
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

	pCocoaMgr = SymbolAddr<void *>(info.dli_fbase, launcher_syms, 1);

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
#if defined(ENGINE_ND) || defined(ENGINE_L4D2)
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
	
	if (!BlockSteamService())
		return 0;

	return ret;
#else
	int ret;
	
	/* Call the original */
	ret = DETOUR_MEMBER_CALL(CSys_LoadModules)(appsys);
	
	if (!BlockSteamService())
		return 0;
	
	return ret;
#endif
}
#endif // !ENGINE_INS && !ENGINE_DOTA

#endif

bool DoDedicatedHacks(void *entryPoint)
{
#if defined(ENGINE_CSGO)
	/* Detour CSys::LoadModules() */
	detSysLoadModules = DETOUR_CREATE_MEMBER(CSys_LoadModules, dedicated_syms_[0].address);
	if (!detSysLoadModules)
	{
		printf("Failed to create detour for CSys::LoadModules\n");
		return false;
	}
	detSysLoadModules->EnableDetour();
	return true;
#else

	void *tier0, *dbgString;
#if !defined(ENGINE_DOTA)
	Dl_info info;
	void *sysLoad, *loadModule;
#if !defined(ENGINE_OBV) && !defined(ENGINE_OBV_SDL) && !defined(ENGINE_GMOD)
	void **pFileSystem;
	void **pBaseFileSystem;
	void *fileSystem;
#endif
#if defined(ENGINE_L4D)
	char *exeName;
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
#if defined(ENGINE_L4D)
	exeName = SymbolAddr<char *>(info.dli_fbase, dedicated_syms, 6);
	strncpy(exeName, *_NSGetArgv()[0], 256);
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

#if defined(ENGINE_GMOD) || defined(ENGINE_L4D2) || defined(ENGINE_ND) || defined(ENGINE_OBV_SDL)
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

#endif
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

	if (detSteamLoadModule)
	{
		detSteamLoadModule->Destroy();
	}

	if (detLoadModule)
	{
		detLoadModule->Destroy();
	}

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
bool BuildCmdLine(int argc, char **argv)
{
	Dl_info info;
	void *tier0 = NULL;
	void *msg = NULL;

	tier0 = dlopen("libtier0.dylib", RTLD_NOLOAD);
	if (!tier0)
	{
		printf("Failed to get existing handle for libtier0.dylib (%s)\n", dlerror());
		return false;
	}

	msg = dlsym(tier0, "Msg");
	if (!msg)
	{
		printf("Failed to find Msg (%s)\n", dlerror());
		dlclose(tier0);
		return false;
	}

	if (!dladdr(msg, &info) || !info.dli_fbase || !info.dli_fname)
	{
		printf("Failed to get base address of libtier0.dylib\n");
		dlclose(tier0);
		return false;
	}

	dlclose(tier0);

	char *cmdline = SymbolAddr<char *>(info.dli_fbase, tier0_syms, 0);
	const int maxCmdLine = 512;
	int len = 0;

	for (int i = 0; i < argc; i++)
	{
		/* Account for spaces between command line paramaters and null terminator */
		len += strlen(argv[i]) + 1;
	}

	if (len > maxCmdLine)
	{
		printf("Command line too long, %d max\n", maxCmdLine);
		return false;
	}

	cmdline[0] = '\0';
	for (int i = 0; i < argc; i++)
	{
		if (i > 0)
			strcat(cmdline, " ");
		strcat(cmdline, argv[i]);
	}


	return true;
}
#endif
