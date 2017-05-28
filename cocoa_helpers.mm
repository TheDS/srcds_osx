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
	bool found = false;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];	
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	NSString *appSupport = [paths firstObject];
	
	if (!appSupport)
	{
		[pool release];
		return found;
	}
	
	NSString *steamPath = [appSupport stringByAppendingPathComponent:@"Steam/Steam.AppBundle/Steam/Contents/MacOS"];
	NSFileManager *fileManager = [NSFileManager defaultManager];
	BOOL isDir;
	
	if ([fileManager fileExistsAtPath:steamPath isDirectory:&isDir] && isDir)
	{
		mm_Format(buf, len, "%s", [steamPath UTF8String]);
		found = true;
	}

	[pool release];
	return found;
}
