// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  libintl.h
//

// This is the Visopsys version of the GNU gettext header file libintl.h


#if !defined(_LIBINTL_H)

#define GETTEXT_LOCALEDIR_PREFIX  "/system/locale"
#define GETTEXT_DEFAULT_DOMAIN    "messages"

char *bindtextdomain(const char *, const char *);
char *gettext(const char *);
char *textdomain(const char *);
      
#define _LIBINTL_H
#endif
