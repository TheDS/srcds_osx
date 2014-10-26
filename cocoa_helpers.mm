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

#import <Foundation/Foundation.h>
#import <AppKit/NSWorkspace.h>
#import "mm_util.h"
#import "cocoa_helpers.h"

bool GetSteamPath(char *buf, size_t len)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSWorkspace *env = [NSWorkspace sharedWorkspace];
	NSString *app = [env absolutePathForAppBundleWithIdentifier:@"com.valvesoftware.steam"];
	
	if (!app)
	{
		[pool release];
		return false;
	}

	mm_Format(buf, len, "%s/Contents/MacOS", [app UTF8String]);

	[pool release];
	return true;
}
