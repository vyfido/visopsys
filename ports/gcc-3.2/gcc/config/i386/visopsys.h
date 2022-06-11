/* Configuration for an i386 running Visopsys
   Copyright (C) 1994 Free Software Foundation, Inc.
   Contributed by Andy McLaughlin <andy@visopsys.org>.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#define DBX_DEBUGGING_INFO 
#define SDB_DEBUGGING_INFO 
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

#include "i386/gas.h"
#include "dbxcoff.h"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (i386 Visopsys/ELF)");

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-DVISOPSYS -Asystem=visopsys"

#undef SYSTEM_INCLUDE_DIR
#define SYSTEM_INCLUDE_DIR "/usr/local/i386-pc-visopsys/include/sys/"

#undef STANDARD_INCLUDE_DIR
#define STANDARD_INCLUDE_DIR "/usr/local/i386-pc-visopsys/include/"

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX "/usr/local/i386-pc-visopsys/lib/"

#undef  STARTFILE_SPEC
#define STARTFILE_SPEC "crt0.o%s"

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC ""

#undef LINK_SPEC
#define LINK_SPEC "-warn-common -X --oformat elf32-i386 -Ttext 0x0 -L/usr/local/i386-pc-visopsys/lib/"

#undef LIB_SPEC
#define LIB_SPEC "-lc"

