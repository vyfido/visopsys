//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  ls.c
//

// Yup, it's the UNIX-style command for viewing directory listings

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/api.h>


static char cwd[MAX_PATH_LENGTH];


/*
static void usage(char *name)
{
  printf("usage:\n");
  printf("%s [item 1] ...\n", name);
  return;
}
*/

static void makeAbsolutePath(const char *orig, char *new)
{

  if ((orig[0] != '/') && (orig[0] != '\\'))
    {
      strcpy(new, cwd);

      if ((new[strlen(new) - 1] != '/') &&
	  (new[strlen(new) - 1] != '\\'))
	strncat(new, "/", 1);

      strcat(new, orig);
    }
  else
    strcpy(new, orig);

  return;
}


static void printTime(unsigned int unformattedTime)
{
  int seconds = 0;
  int minutes = 0;
  int hours = 0;

  seconds = (unformattedTime & 0x0000003F);
  minutes = ((unformattedTime & 0x00000FC0) >> 6);
  hours = ((unformattedTime & 0x0003F000) >> 12);

  if (hours < 10)
    putchar('0');
  printf("%u:", hours);
  if (minutes < 10)
    putchar('0');
  printf("%u:", minutes);
  if (seconds < 10)
    putchar('0');
  printf("%u", seconds);

  return;
}


static void printDate(unsigned int unformattedDate)
{
  int day = 0;
  int month = 0;
  int year = 0;


  day = (unformattedDate & 0x0000001F);
  month = ((unformattedDate & 0x000001E0) >> 5);
  year = ((unformattedDate & 0xFFFFFE00) >> 9);

  switch(month)
    {
    case 1:
      printf("Jan");
      break;
    case 2:
      printf("Feb");
      break;
    case 3:
      printf("Mar");
      break;
    case 4:
      printf("Apr");
      break;
    case 5:
      printf("May");
      break;
    case 6:
      printf("Jun");
      break;
    case 7:
      printf("Jul");
      break;
    case 8:
      printf("Aug");
      break;
    case 9:
      printf("Sep");
      break;
    case 10:
      printf("Oct");
      break;
    case 11:
      printf("Nov");
      break;
    case 12:
      printf("Dec");
      break;
    default:
      printf("???");
      break;
    }

  putchar(' ');

  if (day < 10)
    putchar('0');
  printf("%u %u", day, year);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  int argNumber = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  unsigned int bytesFree = 0;
  int numberFiles = 0;
  file theFile;
  int count;


  // Get the current directory
  multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

  // If we got no arguments, then we assume we are operating on the
  // current directory.
  if (argc == 1)
    {
      argv[argc] = cwd;
      argc++;
    }

  // Do a separate loop for each filename argument we were given
  for (argNumber = 1; argNumber < argc; argNumber ++)
    {
      if (argv[argNumber] == NULL)
	return (status = ERR_NULLPARAMETER);

      // If the filename argument is relative, we should fix it up
      makeAbsolutePath(argv[argNumber], fileName);

      // Initialize the file structure
      for (count = 0; count < sizeof(file); count ++)
	((char *) &theFile)[count] = NULL;

      // Call the "find file" routine to see if the file exists
      status = fileFind(fileName, &theFile);
  
      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}

      // Get the bytes free for the filesystem
      bytesFree = filesystemGetFree(theFile.filesystem);

      // We do things differently depending upon whether the target is a 
      // file or a directory

      if (theFile.type == fileT)
	{
	  // This means the item is a single file.  We just output
	  // the appropriate information for that file
	  printf(theFile.name);

	  if (strlen(theFile.name) < 24)
	    for (count = 0; count < (26 - strlen(theFile.name)); 
		 count ++)
	      putchar(' ');
	  else
	    printf("  ");

	  // The date and time
	  printDate(theFile.modifiedDate);
	  putchar(' ');
	  printTime(theFile.modifiedTime);
	  printf("    ");

	  // The file size
	  printf("%u\n", theFile.size);
	}

      else
	{
	  printf("\n  Directory of %s\n", (char *) fileName);

	  // Get the first file
	  status = fileFirst(fileName, &theFile);
  
	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	      return (status);
	    }

	  while (1)
	    {
	      printf(theFile.name);

	      if (theFile.type == dirT)
		putchar('/');
	      else 
		putchar(' ');
	      
	      if (strlen(theFile.name) < 23)
		for (count = 0; count < (25 - strlen(theFile.name)); 
		     count ++)
		  putchar(' ');
	      else
		printf("  ");
	      
	      // The date and time
	      printDate(theFile.modifiedDate);
	      putchar(' ');
	      printTime(theFile.modifiedTime);
	      printf("    ");

	      // The file size
	      printf("%u\n", theFile.size);

	      numberFiles += 1;
	      
	      status = fileNext(fileName, &theFile);

	      if (status < 0)
		break;
	    }
      
	  printf("  ");

	  if (numberFiles == 0)
	    printf("No");
	  else
	    printf("%u", numberFiles);
	  printf(" file");
	  if ((numberFiles == 0) || (numberFiles > 1))
	    putchar('s');
	  printf("\n");

	  printf("  %u bytes free\n\n", bytesFree);
	}
    }
  
  // Return success
  return (status = 0);
}
