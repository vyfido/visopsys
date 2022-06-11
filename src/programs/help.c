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
//  help.c
//

// This is like the UNIX-style 'man' command for showing documentation

/* This is the text that appears when a user requests help about this program
<help>

 -- List of commands (type 'help <command>' for specific help) --

adduser           Add a user account to the system
bootmenu          Install or edit the boot loader menu
cal               Display the days of the current calendar month
cat (or type)     Print a file's contents on the screen
cd                Change the current directory
cdrom             Control of the CD-ROM device, such as opening and closing
chkdisk           Check a filesystem for errors
copy-boot         Write a Visopsys boot sector
copy-mbr          Write a Visopsys MBR sector
cp (or copy)      Copy a file
date              Show the date
defrag            Defragment a filesystem
deluser           Delete a user account from the system
disks             Show the disk volumes in the system
domainname        Print or set the system's network domain name
fdisk             Manage hard disks (must be user "admin")
file              Show the type of a file
find              Traverse directory hierarchies
fontutil          Edit and convert Visopsys fonts
format            Create new, empty filesystems
help              Show this summary of help entries
hexdump           View files as hexadecimal listings
host              Look up network names and addresses
hostname          Print or set the system's network host name
ifconfig          Network device information and control
imgboot           The program launched at first system boot
install           Install Visopsys (must be user "admin")
keymap            View or change the current keyboard mapping
kill              Kill a running process
login             Start a new login process
logout (or exit)  End the current session
ls (or dir)       Show the files in a directory
lsdev             Display devices
md5               Calculate and print an md5 digest
mem               Show system memory usage
mkdir             Create one or more new directories
more              Display file's contents, one screenfull at a time
mount             Mount a filesystem
mv (or move)      Move a file (ren or rename have the same effect)
netsniff          Sniff network packets
netstat           Show network connections
nm                Show symbol information for a dynamic program or library
passwd            Set the password on a user account
ping              'Ping' a host on the network
ps                Show list of current processes
pwd               Show the current directory
ramdisk           Create or destroy RAM disks
reboot            Reboot the computer
renice            Change the priority of a running process
rm (or del)       Delete a file
rmdir             Remove a directory
sha1pass          Calculate and print SHA1 digests of strings
sha1sum           Calculate and print SHA1 digests of files
sha256pass        Calculate and print SHA256 digests of strings
sha256sum         Calculate and print SHA256 digests of files
shutdown          Stop the computer
snake             A 'snake' game like the one found on mobile phones
software          Download, install, and update software
store             Server for software downloads
sync              Synchronize all filesystems on disk
sysdiag           Perform system diagnostics
tar               Create or manage archives using the TAR format
telnet            Connect to a remote host using the telnet protocol
touch             Update a file or create a new (empty) file
umount            Unmount a filesystem
uname             Print system information
unzip             Decompress and extract files from a compressed archive file
uptime            Time since last boot
vsh               Start a new command shell
vspinst           Install a software package
vspmake           Create a software package
wget              Download files from the web
who               Show who is logged in
zip               Compress and archive files

 -- Additional (graphics mode only) --

archman           A graphical program for managing archive files
calc              A calculator program
clock             Show a simple clock in the taskbar menu
cmdwin            Open a new command window
computer          Navigate the resources of the computer
confedit          Edit Visopsys configuration files
console           Show the console window
deskwin           A 'desktop'-style window shell
disprops          View or change the display settings
edit              Simple text editor
filebrowse        Navigate the file system
filesys           Set mount points and other filesystem properties
iconwin           A program for displaying custom icon windows
imgedit           Simple image editor
keyboard          Display a virtual keyboard
mines             A mine sweeper game
progman           View and manage programs and processes
screenshot        Take a screenshot
users             User manager for creating/deleting user accounts
view              Display a file in a new window
wallpaper         Load a new background wallpaper image

</help>
*/

#include <ctype.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/file.h>
#include <sys/paths.h>
#include <sys/vsh.h>

#define _(string) gettext(string)
#define MAX_LINELEN 128


static int generalHelp(void)
{
	int status = 0;
	const char *fileName = PATH_PROGRAMS_HELPFILES "/help.txt";
	file theFile;
	char *helpBuffer = NULL;
	fileStream helpFileStream;
	char lineBuffer[MAX_LINELEN];
	unsigned helpLen = 0;
	char command[MAX_LINELEN];
	char commandPath[MAX_PATH_NAME_LENGTH + 1];
	int count;

	// Initialize stack data
	memset(&theFile, 0, sizeof(file));
	memset(&helpFileStream, 0, sizeof(fileStream));

	// See if we can get the help file
	status = fileFind(fileName, &theFile);
	if ((status < 0) || !theFile.size)
	{
		printf("%s", _("There is no general help available\n"));
		return (status);
	}

	// Get a buffer for the output
	helpBuffer = calloc(theFile.size, 1);
	if (!helpBuffer)
		return (status = ERR_MEMORY);

	// Open the help file as a file stream
	status = fileStreamOpen(fileName, OPENMODE_READ, &helpFileStream);
	if (status < 0)
	{
		free(helpBuffer);
		return (status);
	}

	// Read line by line
	while (1)
	{
		status = fileStreamReadLine(&helpFileStream, MAX_LINELEN, lineBuffer);
		if (status < 0)
			// End of file?
			break;

		if (lineBuffer[0])
		{
			// If the line starts with whitespace, just show it
			if (isspace(lineBuffer[0]))
			{
				strcpy((helpBuffer + helpLen), lineBuffer);
				helpLen += strlen(lineBuffer);
			}
			else
			{
				// This line should represent a command.  Isolate the initial,
				// non-whitespace part.
				for (count = 0; !isspace(lineBuffer[count]); count ++)
					command[count] = lineBuffer[count];
				command[count] = '\0';

				// Search the path for it
				if (vshSearchPath(command, commandPath) < 0)
					// Doesn't seem to be present
					continue;

				strcpy((helpBuffer + helpLen), lineBuffer);
				helpLen += strlen(lineBuffer);
			}
		}

		strcpy((helpBuffer + helpLen++), "\n");
	}

	strcpy((helpBuffer + helpLen++), "\n");

	fileStreamClose(&helpFileStream);

	// Show the buffer
	status = vshPageBuffer(helpBuffer, helpLen, _("--More--(%d%%)"));

	free(helpBuffer);

	return (status);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char helpFile[MAX_PATH_NAME_LENGTH + 1];
	int count;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("help");

	if (argc < 2)
	{
		// If there are no arguments, print the general help file
		status = generalHelp();
	}
	else
	{
		// For each argument, look for a help file whose name matches
		for (count = 1; count < argc; count ++)
		{
			// See if there is a help file for the argument

			sprintf(helpFile, "%s/%s.txt", PATH_PROGRAMS_HELPFILES,
				argv[count]);

			status = fileFind(helpFile, NULL);
			if (status < 0)
			{
				// No help file
				printf(_("There is no help available for \"%s\"\n"),
					argv[count]);
				return (status = ERR_NOSUCHFILE);
			}

			status = vshPageFile(helpFile, _("--More--(%d%%)"));
			if (status < 0)
				break;
		}
	}

	return (status);
}

