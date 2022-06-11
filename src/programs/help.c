//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  help.c
//

// This is like the UNIX-style 'man' command for showing documentation

/* This is the text that appears when a user requests help about this program
<help>

 -- List of commands (type 'help <command>' for specific help) --

adduser           Add a user account to the system
cat (or type)     Print a file's contents on the screen
cd                Change the current directory
cdrom             Control of the CD-ROM device, such as opening and closing
chkdisk           Check a filesystem for errors
cp (or copy)      Copy a file
date              Show the date
disks             Show the disk volumes in the system
fdisk             Manage hard disks (must be user "admin")
find              Traverse directory hierarchies
format            Create new, empty filesystems
imgboot           The 'first boot' program that asks if you want to install
install           Install Visopsys (must be user "admin")
keymap            View or change the current keyboard mapping
kill              Kill a running process
login             Start a new login process
logout (or exit)  End the current session
ls (or dir)       Show the files in a directory
md5               Calculate and print an md5 digest
mem               Show system memory usage
mkdir             Create one or more new directories
more              Display file's contents, one screenfull at a time
mount             Mount a filesystem
mv (or move)      Move a file (ren or rename have the same effect)
passwd            Set the password on a user account
ps                Show list of current processes
pwd               Show the current directory
reboot            Exits to real mode and reboots the computer
renice            Change the priority of a running process
rm (or del)       Delete a file
rmdir             Remove a directory
shutdown          Stops the computer
sync              Synchronize all filesystems on disk
touch             Update a file or create a new (empty) file
umount            Unmount a filesystem
uname             Prints the Visopsys version
uptime            Time since last boot
vsh               Start a new command shell

 -- Additional (graphics mode only) --

clock             Show a simple clock in the corner of the screen.
confedit          Edit Visopsys configuration files
console           Show the console window
disprops          View or change the display settings
loadfont          Load a new default font
screenshot        Take a screenshot
users             User manager for creating/deleting user accounts
view              Show an image file in a new window
wallpaper         Load a new background wallpaper image
window            Open a new command window

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>

#define HELPFILES_DIR  "/programs/helpfiles"


int main(int argc, char *argv[])
{
  int status = 0;
  char command[MAX_PATH_NAME_LENGTH];
  file tmpFile;
  int count;

  if (argc < 2)
    // If there are no arguments, print the general help file
    status = system("/programs/more " HELPFILES_DIR "/help.txt");

  else
    {
      for (count = 1; count < argc; count ++)
	{
	  // See if there is a help file for the argument
	  sprintf(command, "%s/%s.txt", HELPFILES_DIR, argv[count]);
	  status = fileFind(command, &tmpFile);
	  if (status < 0)
	    {
	      // No help file
	      printf("There is no help available for \"%s\"\n", argv[count]);
	      return (status = ERR_NOSUCHFILE);
	    }

	  // For each argument, look for a help file whose name matches
	  sprintf(command, "/programs/more %s/%s.txt", HELPFILES_DIR,
		  argv[count]);

	  // Search
	  status = system(command);
	  if (status < 0)
	    break;
	}
    }

  return (status);
}
