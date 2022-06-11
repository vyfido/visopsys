//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
//
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  paths.h
//

// This file contains default filesystem paths in Visopsys.

#ifndef _PATHS_H
#define _PATHS_H

// The default programs directory and sub-directories
#define PATH_PROGRAMS			"/programs"
#define PATH_PROGRAMS_HELPFILES	PATH_PROGRAMS "/helpfiles"

// The default system directory and sub-directories
#define PATH_SYSTEM				"/system"
#define PATH_SYSTEM_BOOT		PATH_SYSTEM "/boot"
#define PATH_SYSTEM_CONFIG		PATH_SYSTEM "/config"
#define PATH_SYSTEM_FONTS		PATH_SYSTEM "/fonts"
#define PATH_SYSTEM_HEADERS		PATH_SYSTEM "/headers"
#define PATH_SYSTEM_ICONS		PATH_SYSTEM "/icons"
#define PATH_SYSTEM_KEYMAPS		PATH_SYSTEM "/keymaps"
#define PATH_SYSTEM_LIBRARIES	PATH_SYSTEM "/libraries"
#define PATH_SYSTEM_LOCALE		PATH_SYSTEM "/locale"
#define PATH_SYSTEM_MOUSE		PATH_SYSTEM "/mouse"
#define PATH_SYSTEM_WALLPAPER	PATH_SYSTEM "/wallpaper"

// The default temporary directory
#define PATH_TEMP				"/temp"

// The default parent and sub-directories for user home directories
#define PATH_USERS				"/users"
#define PATH_USERS_HOME			PATH_USERS "/%s"
#define PATH_USERS_CONFIG		PATH_USERS_HOME "/config"

#endif

