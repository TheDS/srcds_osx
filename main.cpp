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
#include <signal.h>
#include <execinfo.h>

#include "platform.h"
#include "hacks.h"
#include "mm_util.h"
#include "cocoa_helpers.h"

#if defined(PLATFORM_X64)
#define INSTR_PTR __rip
#else
#define INSTR_PTR __eip
#endif
//#define STEAMWORKS_CLIENT_INTERFACES
//#include "osw/Steamclient.h"

typedef int (*DedicatedMain_t)(int argc, char **argv);
extern void *g_Launcher;

#if defined(ENGINE_CSGO)
extern void *g_EmptyShader;
#endif

void crash_handler(int sig, siginfo_t *info, void *context)
{
	void *stack[128];
	ucontext_t *cx = (ucontext_t *)context;
	void *eip = (void *)cx->uc_mcontext->__ss.INSTR_PTR;

	fprintf(stderr, "Caught signal %d (%s). Invalid memory access of %p from %p.\n", sig, strsignal(sig), info->si_addr, eip);

	int nframes = backtrace(stack, sizeof(stack) / sizeof(void *)) - 1;

	stack[2] = eip;

	char **frames = backtrace_symbols(stack, nframes);

	if (!frames)
	{
		fprintf(stderr, "Failed to generate backtrace!\n");
		exit(sig);
	}

	fprintf(stderr, "Backtrace:\n");
	for (int i = 0; i < nframes; i++)
		fprintf(stderr, "#%s\n", frames[i]);

	free(frames);

	exit(sig);
}

int main(int argc, char **argv)
{
	bool shouldHandleCrash = false;

	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], "-nobreakpad") == 0)
		{
			shouldHandleCrash = true;
			break;
		}
	}

	// Catch
	if (shouldHandleCrash)
	{
		stack_t sigstack;
		sigstack.ss_sp = malloc(SIGSTKSZ);
		sigaltstack(&sigstack, NULL);
		struct sigaction sa;
		sa.sa_sigaction = crash_handler;
		sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
		sigemptyset(&sa.sa_mask);

		sigaction(SIGSEGV, &sa, NULL);
		sigaction(SIGBUS, &sa, NULL);
	}

	char steamPath[PATH_MAX];

	GetSteamPath(steamPath, sizeof(steamPath));

	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd)))
	{
		printf("getcwd failed (%s)\n", strerror(errno));
		return -1;
	}
	
#if defined(PLATFORM_X86)
	/* Initialize symbol offsets for various libraries that we will be using */
	if (!InitSymbolData(steamPath))
	{
		return -1;
	}
#endif

	char libPath[PATH_MAX];
	char *oldPath = getenv("DYLD_LIBRARY_PATH");
#if defined(PLATFORM_X64)
	mm_Format(libPath, sizeof(libPath), "%s:%s/bin:%s/bin/osx64:%s:%s", cwd, cwd, cwd, steamPath, oldPath);
#else
	mm_Format(libPath, sizeof(libPath), "%s:%s/bin:%s:%s", cwd, cwd, steamPath, oldPath);
#endif
	if (SetLibraryPath(libPath) != 0)
	{
		printf("Failed to set library path!\n");
		return -1;
	}
	
#if defined(PLATFORM_X64)
	/* Initialize symbol offsets for various libraries that we will be using */
	if (!InitSymbolData(steamPath))
	{
		return -1;
	}
#endif

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

	if (!DoDedicatedHacks((void *)DedicatedMain))
	{
		dlclose(lib);
		return -1;
	}

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
	/* Game does not parse command line without this */
	if (!BuildCmdLine(argc, argv))
	{
		dlclose(lib);
		delete[] execPath;
		return -1;
	}
#endif

	int result = DedicatedMain(argc, argv);

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
