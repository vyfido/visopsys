//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  install.h
//

// This file contains public definitions and structures used by the software
// installer library.

#ifndef _INSTALL_H
#define _INSTALL_H

#ifndef PORTABLE
	#include <sys/paths.h>
#endif

#define INSTALL_PACKAGE_MAGIC		0x5053562E // ".VSP" in ASCII
#define INSTALL_PACKAGE_VSP_1_0		0x00010000
#define INSTALL_PACKAGE_EXT			"vsp"
#define INSTALL_PACKAGE_NAMEFORMAT	"%s-%s-%s." INSTALL_PACKAGE_EXT
#define INSTALL_TMP_NAMEFORMAT		"installtmp-XXXXXX"

#define INSTALL_NAME				"name"
#define INSTALL_DESC				"desc"
#define INSTALL_VERSION				"version"
#define INSTALL_ARCH				"arch"
#define INSTALL_DEPEND				"depend"
#define INSTALL_OBSOLETE			"obsolete"
#define INSTALL_PREEXEC				"preexec"
#define INSTALL_POSTEXEC			"postexec"
#define INSTALL_FILE				"file"

#define INSTALL_ARCH_UNKNOWN		"unknown"
#define INSTALL_ARCH_ALL			"all"
#define INSTALL_ARCH_X86			"x86"
#define INSTALL_ARCH_X86_64			"x86_64"

#define INSTALL_SIG_LEN				256
#define INSTALL_VERSION_FIELDS		4
#define INSTALL_VERSION_MAX			48
#define INSTALL_ARCH_MAX			16

// The default software installation database
#define INSTALL_DB_FILE				"db"
#define INSTALL_DB_PATH				PATH_SYSTEM_INSTALL "/" INSTALL_DB_FILE

typedef enum {
	arch_unknown,
	arch_all,
	arch_x86,
	arch_x86_64

} installArch;

typedef enum {
	rel_unknown,
	rel_less,
	rel_lessEqual,
	rel_equal,
	rel_greaterEqual,
	rel_greater

} installRel;

//
// Install package file structure:
//
// --- included in installPackageHeader.headerLen ---
// 1. installPackageHeader
// 2. installDependHeader array [installPackageHeader.numDepends]
// 3. installObsoleteHeader array [installPackageHeader.numObsoletes]
// 4. installFileHeader array [installPackageHeader.numFiles]
// 5. string data (various names)
// --- at offset installPackageHeader.headerLen ---
// 6. gzipped tar file
//

// The file header for installer packages
typedef struct {
	unsigned vspMagic;
	unsigned vspVersion;
	unsigned headerLen;
	unsigned packageName;	// file offset
	unsigned packageDesc;	// file offset
	int packageVersion[INSTALL_VERSION_FIELDS];
	installArch packageArch;
	unsigned preExec;		// file offset
	unsigned postExec;		// file offset
	int numDepends;
	int numObsoletes;
	int numFiles;
	unsigned char signature[INSTALL_SIG_LEN];

} __attribute__((packed)) installPackageHeader;

// The subheader for installer package dependencies
typedef struct {
	unsigned name;			// file offset
	installRel rel;
	int version[INSTALL_VERSION_FIELDS];

} __attribute__((packed)) installDependHeader;

// The subheader for installer package obsolescences
typedef installDependHeader installObsoleteHeader;

// The subheader for each file inside installer packages
typedef struct {
	unsigned archiveName;	// file offset
	unsigned targetName;	// file offset

} __attribute__((packed)) installFileHeader;

// In-memory representation of installer package dependencies
typedef struct {
	char *name;
	installRel rel;
	char version[INSTALL_VERSION_MAX];

} installDepend;

// In-memory representation of installer package obsolescences
typedef installDepend installObsolete;

// In-memory representation of each file inside installer packages
typedef struct {
	char *archiveName;
	char *targetName;

} installFile;

// In-memory representation of an installer package
typedef struct {
	char *name;
	char *desc;
	char version[INSTALL_VERSION_MAX];
	char arch[INSTALL_ARCH_MAX];
	char *preExec;
	char *postExec;
	int numDepends;
	installDepend *depend;
	int numObsoletes;
	installObsolete *obsolete;
	int numFiles;
	installFile *file;
	void *data;

} installInfo;

// Functions exported by libinstall

// Portable
int installHeaderInfoGet(unsigned char *, installInfo **);
int installPackageInfoGet(const char *, installInfo **);
void installInfoFree(installInfo *);
void installVersion2String(int *, char *);
void installString2Version(const char *, int *);
const char *installArch2String(installArch);
installArch installString2Arch(const char *);
const char *installRel2String(installRel);
installRel installString2Rel(const char *);
int installVersionIsNewer(int *, int *);
int installVersionStringIsNewer(const char *, const char *);
int installManifestBufferInfoCreate(char *, unsigned, installInfo **);
int installManifestFileInfoCreate(const char *, installInfo **);
int installWrapArchivePackage(installInfo *, const char *, const char *);
int installUnwrapPackageArchive(const char *, installInfo **, char **);
int installDatabaseRead(installInfo ***, int *, const char *);
int installDatabaseWrite(installInfo **, int, const char *);
int installCheck(installInfo *, const char *);
int installPackageAdd(installInfo *, const char *, const char *);
int installPackageRemove(installInfo *, const char *);

// Visopsys-only (requires Visopsys libcompress)
int installArchiveCreate(installInfo *, char **);
int installArchiveExtract(const char *, char **);

#endif

