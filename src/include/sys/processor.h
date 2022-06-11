//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  processor.h
//

#ifndef _PROCESSOR_H
#define _PROCESSOR_H

// This file generically includes processor-specific definitions and
// operations

#ifdef ARCH_X86
	#include <arch/x86/processor.h>

	// Paging constants
	#define PROCESSOR_PAGE_TABLES_PER_DIR	X86_PAGE_TABLES_PER_DIR
	#define PROCESSOR_PAGES_PER_TABLE		X86_PAGES_PER_TABLE

	// Page entry bitfield values
	#define PROCESSOR_PAGEFLAG_PRESENT		X86_PAGEFLAG_PRESENT
	#define PROCESSOR_PAGEFLAG_WRITABLE		X86_PAGEFLAG_WRITABLE
	#define PROCESSOR_PAGEFLAG_USER			X86_PAGEFLAG_USER
	#define PROCESSOR_PAGEFLAG_WRITETHROUGH	X86_PAGEFLAG_WRITETHROUGH
	#define PROCESSOR_PAGEFLAG_CACHEDISABLE	X86_PAGEFLAG_CACHEDISABLE
	#define PROCESSOR_PAGEFLAG_ACCESSED		X86_PAGEFLAG_ACCESSED
	#define PROCESSOR_PAGEFLAG_DIRTY		X86_PAGEFLAG_DIRTY
	#define PROCESSOR_PAGEFLAG_PAT			X86_PAGEFLAG_PAT
	#define PROCESSOR_PAGEFLAG_GLOBAL		X86_PAGEFLAG_GLOBAL

#else
	#error "ARCH not defined or not supported"
#endif

#endif

