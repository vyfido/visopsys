//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  test.c
//

// This is a test driver program.

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/vsh.h>

// Tests should save/restore the text screen if they (deliberately) spew
// output or errors.
static textScreen screen;

#define MAXFAILMESS 80
static char failMess[MAXFAILMESS];
#define FAILMESS(message, arg...) \
  snprintf(failMess, MAXFAILMESS, message, ##arg)


static int format_strings(void)
{
  // Tests the C library's handling of printf/scanf -style format strings

  int status = 0;
  char format[80];
  char buff[80];
  unsigned long long val[2];
  char charVal[2];
  char stringVal[8];
  int typeCount, widthCount, count;

  struct {
    const char *spec;
    int bits;
    int sign;
  } types[] = {
    { "d", 32, 1 },
    { "lld", 64, 1 },
    { "u", 32, 0 },
    { "llu", 64, 0 },
    { "p", 32, 0 },
    { "x", 32, 0 },
    { "X", 32, 0 },
    { NULL, 0, 0, }
  };

  for (typeCount = 0; types[typeCount].spec; typeCount ++)
    {
      for (widthCount = 0; widthCount < 4; widthCount ++)
	{
	  strcpy(format, "foo %");

	  if (widthCount == 3)
	    strcat(format, "-");
	  if (widthCount == 2)
	    strcat(format, "0");
	  if (widthCount > 0)
	    {
	      if (types[typeCount].bits == 32)
		strcat(format, "8");
	      else if (types[typeCount].bits == 64)
		strcat(format, "16");
	    }

	  strcat(format, types[typeCount].spec);
	  strcat(format, " bar");

	  for (count = 0; count < 100; count ++)
	    {
	      val[0] = (unsigned long long) randomUnformatted();
	      if (types[typeCount].bits == 64)
		{
		  val[0] <<= 32;
		  val[0] |= randomUnformatted();
		}

	      if (types[typeCount].bits == 32)
		status = snprintf(buff, 80, format, (unsigned) val[0]);
	      else
		status = snprintf(buff, 80, format, val[0]);

	      if (status < 0)
		{
		  FAILMESS("Error expanding \"%s\" format", format);
		  goto out;
		}

	      val[1] = 0;
	      if (types[typeCount].bits == 32)
		status = sscanf(buff, format, (unsigned *) &(val[1]));
	      else if (types[typeCount].bits == 64)
		status = sscanf(buff, format, &(val[1]));

	      if (status < 0)
		{
		  FAILMESS("Error code %d while reading \"%s\" input", status,
			   format);
		  goto out;
		}

	      if (status != 1)
		{
		  FAILMESS("Couldn't read \"%s\" input", format);
		  status = ERR_INVALID;
		  goto out;
		}

	      if (val[1] != val[0])
		{
		  if (types[typeCount].sign)
		    FAILMESS("\"%s\" output %lld does not match input %lld",
			     format, val[1], val[0]);
		  else
		    FAILMESS("\"%s\" output %llu does not match input %llu",
			     format, val[1], val[0]);
		  status = ERR_INVALID;
		  goto out;
		}
	    }
	}
    }

  for (count = 0; count < 10; count ++)
    {
      // Test character output first, then input, by reading back the output
      charVal[0] = randomFormatted(' ', '~');
      strcpy(format, "%c");
      status = snprintf(buff, 80, format, charVal[0]);
      if (status < 0)
	{
	  FAILMESS("Error expanding char format");
	  goto out;
	}
      status = sscanf(buff, format, &(charVal[1]));
      if (status < 0)
	{
	  FAILMESS("Error formatting char input");
	  goto out;
	}
      if (status != 1)
	{
	  FAILMESS("Error formatting char input");
	  status = ERR_INVALID;
	  goto out;
	}
      if (charVal[0] != charVal[1])
	{
	  FAILMESS("Char output '%c' does not match input '%c'",
		   charVal[1], charVal[0]);
	  status = ERR_INVALID;
	  goto out;
	}
    }

  // Test string output first, then input, by reading back the output
  strcpy(format, "%s");
  status = snprintf(buff, 80, format, "FOOBAR!");
  if (status < 0)
    {
      FAILMESS("Error expanding string format");
      goto out;
    }
  status = sscanf(buff, format, stringVal);
  if (status < 0)
    {
      FAILMESS("Error formatting string input");
      goto out;
    }
  if (status != 1)
    {
      FAILMESS("Error formatting string input");
      status = ERR_INVALID;
      goto out;
    }
  if (strcmp(stringVal, "FOOBAR!"))
    {
      FAILMESS("String output %s does not match input %s",
	       stringVal, "FOOBAR!");
      status = ERR_INVALID;
      goto out;
    }

  status = 0;

 out:
  return (status);
}


static int crashThread(void)
{
  // This deliberately causes a divide-by-zero exception.

  int a = 1;
  int b = 0;

  return (a / b);
}


static int exceptions(void)
{
  // Tests the kernel's exception handing.

  int status = 0;
  int procId = 0;
  int count = 0;

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    goto out;

  for (count = 0; count < 10; count ++)
    {
      procId = multitaskerSpawn(&crashThread, "crashy thread", 0, NULL);
      if (procId < 0)
	return (status = procId);

      // Let it run
      multitaskerYield();
      multitaskerYield();

      if (graphicsAreEnabled())
	// Try to get rid of any exception dialogs we caused
	multitaskerKillByName("error dialog thread", 0);

      // Now it should be dead
      if (multitaskerProcessIsAlive(procId))
	{
	  FAILMESS("Kernel did not kill exception-causing process");
	  status = ERR_INVALID;
	  goto out;
	}
    }

  status = 0;

 out:

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


static int text_output(void)
{
  // Does a bunch of text-output testing.

  int status = 0;
  int columns = 0;
  int rows = 0;
  int screenFulls = 0;
  int bufferSize = 0;
  char *buffer = NULL;
  int length = 0;
  int count;

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    goto out;

  columns = textGetNumColumns();
  if (columns < 0)
    {
      status = columns;
      goto out;
    }
  rows = textGetNumRows();
  if (rows < 0)
    {
      status = rows;
      goto out;
    }

  // How many screenfulls of data?
  screenFulls = 5;

  // Allocate a buffer 
  bufferSize = (columns * rows * screenFulls);
  buffer = malloc(bufferSize);
  if (buffer == NULL)
    {
      FAILMESS("Error getting memory");
      status = ERR_MEMORY;
      goto out;
    }

  // Fill it with random printable data
  for (count = 0; count < bufferSize; count ++)
    {
      char tmp = (char) randomFormatted(32, 126);

      // We don't want '%' format characters 'cause that could cause
      // unpredictable results
      if (tmp == '%')
	{
	  count -= 1;
	  continue;
	}

      buffer[count] = tmp;
    }

  // Randomly sprinkle newlines and tabs
  for (count = 0; count < (screenFulls * 10); count ++)
    {
      buffer[randomFormatted(0, (bufferSize - 1))] = '\n';
      buffer[randomFormatted(0, (bufferSize - 1))] = '\t';
    }

  // Print the buffer
  for (count = 0; count < bufferSize; count ++)
    {
      // Look out for tab characters
      if (buffer[count] == (char) 9)
	{
	  status = textTab();
	  if (status < 0)
	    goto out;
	}

      // Look out for newline characters
      else if (buffer[count] == (char) 10)
	textNewline();

      else
	{
	  status = textPutc(buffer[count]);
	  if (status < 0)
	    goto out;
	}
    }

  // Stick in a bunch of NULLs
  for (count = 0; count < (screenFulls * 30); count ++)
    buffer[randomFormatted(0, (bufferSize - 1))] = '\0';
  buffer[bufferSize - 1] = '\0';

  // Print the buffer again as lines
  for (count = 0; count < bufferSize; )
    {
      length = strlen(buffer + count);
      if (length < 0)
	{
	  // Maybe we made a string that's too long
	  buffer[count + MAXSTRINGLENGTH - 1] = '\0';

	  length = strlen(buffer + count);
	  if (length < 0)
	    {
	      status = length;
	      goto out;
	    }
	}

      status = textPrintLine(buffer + count);
      if (status < 0)
	goto out;

      if (length)
	count += length;
      else
	count += 1;
    }

  status = 0;

 out:
  if (buffer)
    free(buffer);

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


static int text_colors(void)
{
  // Tests the setting/printing of text colors.

  int status = 0;
  int columns = 0;
  color foreground;
  char *buffer = NULL;
  color tmpColor;
  textAttrs attrs;
  int count1, count2;

  color allColors[16] = {
    COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
    COLOR_RED, COLOR_MAGENTA, COLOR_BROWN, COLOR_LIGHTGRAY,
    COLOR_DARKGRAY, COLOR_LIGHTBLUE, COLOR_LIGHTGREEN, COLOR_LIGHTCYAN,
    COLOR_LIGHTRED, COLOR_LIGHTMAGENTA, COLOR_YELLOW, COLOR_WHITE
  };

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    goto out;

  columns = textGetNumColumns();
  if (columns < 0)
    {
      status = columns;
      goto out;
    }

  // Save the current foreground color
  status = textGetForeground(&foreground);
  if (status < 0)
    goto out;

  buffer = malloc(columns);
  if (buffer == NULL)
    {
      FAILMESS("Error getting memory");
      status = ERR_MEMORY;
      goto out;
    }

  for (count1 = 0; count1 < (columns - 1); count1 ++)
    buffer[count1] = '#';
  buffer[columns - 1] = '\0';

  textNewline();

  for (count1 = 0; count1 < 16; count1 ++)
    {
      status = textSetForeground(&allColors[count1]);
      if (status < 0)
	{
	  FAILMESS("Failed to set the foreground color");
	  goto out;
	}

      status = textGetForeground(&tmpColor);
      if (status < 0)
	{
	  FAILMESS("Failed to get the foreground color");
	  goto out;
	}

      if (memcmp(&allColors[count1], &tmpColor, sizeof(color)))
	{
	  FAILMESS("Foreground color not set correctly");
	  status = ERR_INVALID;
	  goto out;
	}

      textPrintLine(buffer);
    }

  for (count1 = 0; count1 < 16; count1 ++)
    for (count2 = 0; count2 < 16; count2 ++)
      {
	bzero(&attrs, sizeof(textAttrs));
	attrs.flags = (TEXT_ATTRS_FOREGROUND | TEXT_ATTRS_BACKGROUND);
	memcpy(&attrs.foreground, &allColors[count1], sizeof(color));
	memcpy(&attrs.background, &allColors[count2], sizeof(color));
	textPrintAttrs(&attrs, buffer);
	textPrintLine("");
      }

  textNewline();

 out:
  if (buffer)
    free(buffer);

  // Restore old foreground color
  textSetForeground(&foreground);

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


static int port_io(void)
{
  // Test IO ports & related stuff!  Davide Airaghi

  int pid = 0;
  int setClear = 0;
  unsigned portN = 0;
  int res = 0;
  unsigned char ch;
  int count;
  
#define inPort8(port, data) \
  __asm__ __volatile__ ("inb %%dx, %%al" : "=a" (data) : "d" (port))    

  pid = multitaskerGetCurrentProcessId();
  if (pid < 0)
    {
      FAILMESS("Error %d getting PID", res);
      goto fail;
    }
  
  for (count = 0; count < 65535; count ++)
    {
      portN = randomFormatted(0, 65535);
      setClear = ((rand() % 2) == 0);

      res = multitaskerSetIOPerm(pid, portN, setClear);	
      if (res < 0)
	{
	  FAILMESS("Error %d setting perms on port %d", res, portN);
	  goto fail;
	}

      if (setClear)
	// Read from the port
	inPort8(portN, ch);

      // Clear permissions again
      res = multitaskerSetIOPerm(pid, portN, 0);	
      if (res < 0)
	{
	  FAILMESS("Error %d clearing perms on port %d", res, portN);
	  goto fail;
	}
    }
    
  return (0);

 fail:
  return (-1);
}


static int disk_reads(disk *theDisk)
{
  int status = 0;
  unsigned startSector = 0;
  unsigned numSectors = 0;
  unsigned char *buffer = NULL;
  int count;

  printf("\nTest reads from disk %s, numSectors %u ", theDisk->name,
	 theDisk->numSectors);

  for (count = 0; count < 1024; count ++)
    {
      numSectors = randomFormatted(1, min(theDisk->numSectors, 512));
      startSector = randomFormatted(0, (theDisk->numSectors - numSectors - 1));

      buffer = malloc(numSectors * theDisk->sectorSize);
      if (buffer == NULL)
	{
	  FAILMESS("Error getting %u bytes disk %s buffer memory",
		   (numSectors * theDisk->sectorSize), theDisk->name);
	  return (status = ERR_MEMORY);
	}

      status = diskReadSectors(theDisk->name, startSector, numSectors, buffer);

      free(buffer);

      if (status < 0)
	{
	  FAILMESS("Error %d reading %u sectors at %u on %s", status,
		   numSectors, startSector, theDisk->name);
	  return (status);
	}
    }
    
  return (status = 0);
}


static int disk_io(void)
{
  // Test disk IO reads

  int status = 0;
  char diskName[DISK_MAX_NAMELENGTH];
  disk theDisk;
  int count = 0;

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    goto fail;

  // Get the logical boot disk name
  status = diskGetBoot(diskName);
  if (status < 0)
    {
      FAILMESS("Error %d getting disk name", status);
      goto fail;
    }

  for (count = 0; count < (DISK_MAX_PARTITIONS + 1); count ++)
    {
      // Take off any partition letter, so that we have the name of the
      // whole physical disk.
      if (isalpha(diskName[strlen(diskName) - 1]))
	diskName[strlen(diskName) - 1] = '\0';

      if (count)
	// Add a partition letter
	sprintf((diskName + strlen(diskName)), "%c", ('a' + count - 1));

      // Get the disk info
      status = diskGet(diskName, &theDisk);
      if (status < 0)
	// No such disk.  Fine.
	break;

      // Do random reads
      status = disk_reads(&theDisk);
      if (status < 0)
	goto fail;
    }

  status = 0;

 fail:
  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


static int file_recurse(const char *dirPath, unsigned startTime)
{
  int status = 0;
  file theFile;
  file tmpFile;
  int numFiles = 0;
  int fileNum = 0;
  char relPath[MAX_PATH_NAME_LENGTH];
  unsigned op = 0;
  char newPath[MAX_PATH_NAME_LENGTH];
  int count;
		  
  // Initialize the file structure
  bzero(&theFile, sizeof(file));

  // Loop through the contents of the directory
  while (rtcUptimeSeconds() < (startTime + 10))
    {
      numFiles = fileCount(dirPath);
      if (numFiles <= 0)
	{
	  FAILMESS("Error %d getting directory %s file count", numFiles,
		   dirPath);
	  return (numFiles);
	}

      if (numFiles <= 2)
	return (status = 0);

      fileNum = randomFormatted(2, (numFiles - 1));
      for (count = 0; count <= fileNum; count ++)
	{
	  if (count == 0)
	    {
	      // Get the first item in the directory
	      status = fileFirst(dirPath, &theFile);
	      if (status < 0)
		{
		  FAILMESS("Error %d finding first file in %s", status,
			   dirPath);
		  return (status);
		}
	    }
	  else
	    {
	      status = fileNext(dirPath, &theFile);
	      if (status < 0)
		{
		  FAILMESS("Error %d finding next file after %s in %s", status,
			   theFile.name, dirPath);
		  return (status);
		}
	    }
	}

      // Construct the relative pathname for this item
      sprintf(relPath, "%s/%s", dirPath, theFile.name);

      // And a new one in case we want to move/rename it
      strncpy(newPath, relPath, MAX_PATH_NAME_LENGTH);
      while (fileFind(newPath, &tmpFile) >= 0)
	snprintf(newPath, MAX_PATH_NAME_LENGTH, "%s/%c%s", dirPath,
		 randomFormatted(65, 90), theFile.name);

      if (theFile.type == dirT)
	{
	  // Randomly decide what type of operation to do to this diretory
	  op = randomFormatted(0, 3);

	  switch (op)
	    {
	    case 0:
	      if (numFiles < 4)
		{
		  // Recursively copy it	      
		  printf("Recursively copy %s to %s\n", relPath, newPath);
		  status = fileCopyRecursive(relPath, newPath);
		  if (status < 0)
		    {
		      FAILMESS("Error %d copying directory %s", status,
			       relPath);
		      return (status);
		    }
		}
	      break;
	    
	    case 1:
	      if (numFiles > 4)
		{
		  // Recursively delete it
		  printf("Recursively delete %s\n", relPath);
		  status = fileDeleteRecursive(relPath);
		  if (status < 0)
		    {
		      FAILMESS("Error %d deleting directory %s", status,
			       relPath);
		      return (status);
		    }
		}
	      break;

	    case 2:
	      // Make a new directory
	      printf("Create %s\n", newPath);
	      status = fileMakeDir(newPath);
	      if (status < 0)
		{
		  FAILMESS("Error %d creating directory %s", status, newPath);
		  return (status);
		}
	      break;
	      
	    case 3:
	      // Recursively process it the normal way
	      status = file_recurse(relPath, startTime);
	      if (status < 0)
		return (status);
	      // Remove the directory.
	      printf("Remove %s\n", relPath);
	      status = fileRemoveDir(relPath);
	      if (status < 0)
		{
		  FAILMESS("Error %d removing directory %s", status, relPath);
		  return (status);
		}
	      break;
	      
	    default:
	      FAILMESS("Unknown op %d for file %s", op, relPath);
	      return (status = ERR_BUG);
	    }
	}
      else
	{
	  // Randomly decide what type of operation to do to this file
	  op = randomFormatted(0, 6);

	  switch (op)
	    {
	    case 0:
	      // Just find the file
	      status = fileFind(relPath, &theFile);
	      if (status < 0)
		{
		  FAILMESS("Error %d finding file %s", status, relPath);
		  return (status);
		}
	      break;

	    case 1:
	      // Read and write the file using block IO
	      printf("Read/write %s (block)\n", relPath);
	      status = fileOpen(relPath, OPENMODE_READWRITE, &theFile);
	      if (status < 0)
		{
		  FAILMESS("Error %d opening file %s", status, relPath);
		  return (status);
		}
	      unsigned char *buffer =
		malloc(theFile.blocks * theFile.blockSize);
	      if (buffer == NULL)
		{
		  FAILMESS("Couldn't get %u bytes memory for file %s",
			   (theFile.blocks * theFile.blockSize), relPath);
		  return (status = ERR_MEMORY);
		}
	      status = fileRead(&theFile, 0, theFile.blocks, buffer);
	      if (status < 0)
		{
		  FAILMESS("Error %d reading file %s", status, relPath);
		  free(buffer);
		  return (status);
		}
	      status = fileWrite(&theFile, 0, theFile.blocks, buffer);
	      if (status < 0)
		{
		  FAILMESS("Error %d writing file %s", status, relPath);
		  free(buffer);
		  return (status);
		}
	      status = fileWrite(&theFile, theFile.blocks, 1, buffer);
	      free(buffer);
	      if (status < 0)
		{
		  FAILMESS("Error %d rewriting file %s", status, relPath);
		  return (status);
		}
	      status = fileClose(&theFile);
	      if (status < 0)
		{
		  FAILMESS("Error %d closing file %s", status, relPath);
		  return (status);
		}
	      break;

	    case 2:
	      // Delete the file
	      printf("Delete %s\n", relPath);
	      status = fileDelete(relPath);
	      if (status < 0)
		{
		  FAILMESS("Error %d deleting file %s", status, relPath);
		  return (status);
		}
	      break;

	    case 3:
	      // Delete the file securely
	      printf("Securely delete %s\n", relPath);
	      status = fileDeleteSecure(relPath, 9);
	      if (status < 0)
		{
		  FAILMESS("Error %d securely deleting file %s", status,
			   relPath);
		  return (status);
		}
	      break;

	    case 4:
	      // Copy the file
	      printf("Copy %s to %s\n", relPath, newPath);
	      status = fileCopy(relPath, newPath);
	      if (status < 0)
		{
		  FAILMESS("Error %d copying file %s to %s", status,
			   relPath, newPath);
		  return (status);
		}
	      break;

	    case 5:
	      // Move the file
	      printf("Move %s to %s\n", relPath, newPath);
	      status = fileMove(relPath, newPath);
	      if (status < 0)
		{
		  FAILMESS("Error %d moving file %s to %s", status,
			   relPath, newPath);
		  return (status);
		}
	      break;

	    case 6:
	      printf("Timestamp file %s\n", relPath);
	      status = fileTimestamp(relPath);
	      if (status < 0)
		{
		  FAILMESS("Error %d timestamping file %s", status,
			   relPath);
		  return (status);
		}
	      break;

	    default:
	      FAILMESS("Unknown op %d for file %s", op, relPath);
	      return (status = ERR_BUG);
	    }
	}
    }

  // Timed out.
  return (status = 0);
}


static int file_ops(void)
{
  // Test filesystem IO

  int status = 0;
  file theFile;
  unsigned startTime = 0;
  int count;

  char *useFiles[] = { "/programs", "/system", "/visopsys", NULL };
  #define DIRNAME "./test_tmp"

  // Initialize the file structure
  bzero(&theFile, sizeof(file));

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    goto fail;

  // If the test directory exists, delete it
  if (fileFind(DIRNAME, &theFile) >= 0)
    {
      printf("Recursively delete %s\n", DIRNAME);
      status = fileDeleteRecursive(DIRNAME);
      if (status < 0)
	{
	  FAILMESS("Error %d recursively deleting %s", status, DIRNAME);
	  goto fail;
	}
    }

  status = fileMakeDir(DIRNAME);
  if (status < 0)
    {
      FAILMESS("Error %d creating test directory", status);
      goto fail;
    }

  startTime = rtcUptimeSeconds();
  while (rtcUptimeSeconds() < (startTime + 10))
    {
      for (count = 0; useFiles[count]; count ++)
	{
	  char tmpName[MAX_PATH_NAME_LENGTH];
	  sprintf(tmpName, "%s%s", DIRNAME, useFiles[count]);

	  printf("Recursively copy %s to %s\n", useFiles[count], tmpName);
	  status = fileCopyRecursive(useFiles[count], tmpName);
	  if (status < 0)
	    {
	      FAILMESS("Error %d recursively copying files from %s", status,
		       useFiles[count]);
	      goto fail;
	    }
	}

      // Now, recurse the directory, doing random file operations on the
      // contents.
      status = file_recurse(DIRNAME, startTime);
      if (status < 0)
	break;
    }

 fail:
  if (fileFind(DIRNAME, &theFile) >= 0)
    {
      printf("Recursively delete %s\n", DIRNAME);
      fileDeleteRecursive(DIRNAME);
    }

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


static int floats(void)
{
  // Do calculations with floats (test's the kernel's FPU exception handling
  // and state saving).  Adapted from an early version of our JPEG processing
  // code.

  int coefficients[64];
  int u = 0;
  int v = 0;
  int x = 0;
  int y = 0;
  float tempValue = 0;
  float temp[64];

  for (x = 0; x < 64; x ++)
    coefficients[x] = rand();

  bzero(coefficients, (64 * sizeof(int)));
  bzero(temp, (64 * sizeof(float)));

  for (x = 0; x < 8; x++)
    for (y = 0; y < 8; y++)
      for (u = 0; u < 8; u++)
	for (v = 0; v < 8; v++)
	  {
	    tempValue = coefficients[(u * 8) + v] *
	      cosf((2 * x + 1) * u * (float) M_PI / 16.0) *
	      cosf((2 * y + 1) * v * (float) M_PI / 16.0);

	    if (!u)
	      tempValue *= (float) M_SQRT1_2;
	    if (!v)
	      tempValue *= (float) M_SQRT1_2;

	    temp[(x * 8) + y] += tempValue;
	  }

  for (y = 0; y < 8; y++)
    for (x = 0; x < 8; x++)
      {
	coefficients[(y * 8) + x] = (int) (temp[(y * 8) + x] / 4.0 + 0.5);
	coefficients[(y * 8) + x] += 128;
      }

  // If we get this far without crashing, we're golden
  return (0);
}


static int gui(void)
{
  int status = 0;
  int numListItems = 250;
  listItemParameters *listItemParams = NULL;
  objectKey window = NULL;
  componentParameters params;
  objectKey fileMenu = NULL;
  objectKey listList = NULL;
  objectKey buttonContainer = NULL;
  objectKey abcdButton = NULL;
  objectKey efghButton = NULL;
  objectKey ijklButton = NULL;
  int count1, count2, count3;

  #define FILEMENU_SAVE 0
  #define FILEMENU_QUIT 1
  static windowMenuContents fileMenuContents = {
    2,
    {
      { "Save", NULL },
      { "Quit", NULL }
    }
  };

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    {
      FAILMESS("Error %d saving screen", status);
      goto out;
    }

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(), "GUI test window");
  if (window == NULL)
    {
      FAILMESS("Error getting window");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  bzero(&params, sizeof(componentParameters));

  // Create the top 'file' menu
  objectKey menuBar = windowNewMenuBar(window, &params);
  if (menuBar == NULL)
    {
      FAILMESS("Error getting menu bar");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  fileMenu = windowNewMenu(menuBar, "File", &fileMenuContents, &params);
  if (fileMenu == NULL)
    {
      FAILMESS("Error getting menu");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;

  params.orientationX = orient_center;

  status = fontLoad("arial-bold-10.bmp", "arial-bold-10", &(params.font), 0);
  if (status < 0)
    {
      FAILMESS("Error %d getting font", status);
      goto out;
    }

  listItemParams = malloc(numListItems * sizeof(listItemParameters));
  if (listItemParams == NULL)
    {
      FAILMESS("Error getting list parameters memory");
      status = ERR_MEMORY;
      goto out;
    }

  for (count1 = 0; count1 < numListItems; count1 ++)
    {
      for (count2 = 0; count2 < WINDOW_MAX_LABEL_LENGTH; count2 ++)
	listItemParams[count1].text[count2] = '#';
      listItemParams[count1].text[WINDOW_MAX_LABEL_LENGTH - 1] = '\0';
    }

  listList = windowNewList(window, windowlist_textonly, min(10, numListItems),
			   1, 0, listItemParams, numListItems, &params);
  if (listList == NULL)
    {
      FAILMESS("Error getting list component");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  // Make a container component for the buttons
  params.gridX += 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
  params.font = NULL;
  buttonContainer = windowNewContainer(window, "buttonContainer", &params);
  if (buttonContainer == NULL)
    {
      FAILMESS("Error getting button container");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  params.gridX = 0;
  params.gridY = 0;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.flags &= ~WINDOW_COMPFLAG_FIXEDHEIGHT;
  abcdButton = windowNewButton(buttonContainer, "ABCD", NULL, &params);
  if (abcdButton == NULL)
    {
      FAILMESS("Error getting button");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  params.gridY += 1;
  params.padTop = 5;
  efghButton = windowNewButton(buttonContainer, "EFGH", NULL, &params);
  if (efghButton == NULL)
    {
      FAILMESS("Error getting button");
      status = ERR_NOTINITIALIZED;
      goto out;
    }
      
  params.gridY += 1;
  ijklButton = windowNewButton(buttonContainer, "IJKL", NULL, &params);
  if (ijklButton == NULL)
    {
      FAILMESS("Error getting button");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  status = windowSetVisible(window, 1);
  if (status < 0)
    {
      FAILMESS("Error %d setting window visible", status);
      goto out;
    }

  // Run a GUI thread just for fun
  status = windowGuiThread();
  if (status < 0)
    {
      FAILMESS("Error %d starting GUI thread", status);
      goto out;
    }

  // Do a bunch of random selections of the list
  for (count1 = 0; count1 < 100; count1 ++)
    {
      // Fill up our parameters with random printable text
      for (count2 = 0; count2 < numListItems; count2 ++)
	{
	  int numChars = randomFormatted(1, (WINDOW_MAX_LABEL_LENGTH - 1));
	  for (count3 = 0; count3 < numChars; count3 ++)
	    listItemParams[count2].text[count3] =
	      (char) randomFormatted(32, 126);
	  listItemParams[count2].text[numChars] = '\0';
	}

      // Set the list data
      status = windowComponentSetData(listList, listItemParams, numListItems);
      if (status < 0)
	{
	  FAILMESS("Error %d setting list component data", status);
	  goto out;
	}

      for (count2 = 0; count2 < 10; count2 ++)
	{
	  // Random selection, including illegal values
	  int rnd = (randomFormatted(0, (numListItems * 3)) - numListItems);

	  status = windowComponentSetSelected(listList, rnd);

	  // See if the value we cooked up was supposed to be acceptable
	  if ((rnd < -1) || (rnd >= numListItems))
	    {
	      // Illegal value, it should have failed.
	      if (status >= 0)
		{
		  FAILMESS("Selection value %d should fail", rnd);
		  status = ERR_INVALID;
		  goto out;
		}
	    }
	  else if (status < 0)
	    // Legal value, should have succeeded
	    goto out;
	}
    }

  windowGuiStop();

  status = windowDestroy(window);
  window = NULL;
  if (status < 0)
    {
      FAILMESS("Error %d destroying window", status);
      goto out;
    }

  status = 0;

 out:
  if (listItemParams)
    free(listItemParams);

  if (window)
    windowDestroy(window);

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


// This table describes all of the functions to run
struct {
  int (*function)(void);
  const char *name;
  int run;
  int graphics;

} functions[] = {

  // function        name               run graphics
  { format_strings,  "format strings",  0,  0 },
  { exceptions,      "exceptions",      0,  0 },
  { text_output,     "text output",     0,  0 },
  { text_colors,     "text colors",     0,  0 },
  { port_io,         "port io",         0,  0 },
  { disk_io,         "disk io",         0,  0 },
  { file_ops,        "file ops",        0,  0 },
  { floats,          "floats",          0,  0 },
  { gui,             "gui",             0,  1 },
  { NULL, NULL, 0, 0 }
};


static void begin(const char *name)
{
  printf("Testing %s... ", name);
  failMess[0] = '\0';
}


static void pass(void)
{
  printf("passed\n");
}


static void fail(void)
{
  printf("failed");
  if (failMess[0])
    printf("   [ %s ]", failMess);
  printf("\n");
}


static int run(void)
{
  int errors = 0;
  int count;

  for (count = 0; functions[count].function ; count ++)
    {
      if (functions[count].run)
	{
	  begin(functions[count].name);
	  if (functions[count].function() >= 0)
	    pass();
	  else
	    {
	      fail();
	      errors += 1;
	    }
	}
    }

  return (errors);
}


static void usage(char *name)
{
  printf("usage:\n%s [-a] [-l] [test1] [test2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int graphics = graphicsAreEnabled();
  char opt;
  int testCount = 0;
  int errors = 0;
  int count1, count2;

  if (argc <= 1)
    {
      usage(argv[0]);
      return (-1);
    }

  while (strchr("al", (opt = getopt(argc, argv, "al"))))
    {
      // Run all tests?
      if (opt == 'a')
	{
	  for (count1 = 0; functions[count1].function ; count1 ++)
	    if (graphics || !functions[count1].graphics)
	      {
		functions[count1].run = 1;
		testCount += 1;
	      }
	}

      // List all tests?
      if (opt == 'l')
	{
	  printf("\nTests:\n");
	  for (count1 = 0; functions[count1].function ; count1 ++)
	    printf("  \"%s\"%s\n", functions[count1].name,
		   (functions[count1].graphics? " (graphics)" : ""));
	  return (0);
	}
    }

  // Any other arguments should indicate specific tests to run.
  for (count1 = optind; count1 < argc; count1 ++)
    for (count2 = 0; functions[count2].function ; count2 ++)
      if (!strcasecmp(argv[count1], functions[count2].name))
	{
	  if (!graphics && functions[count2].graphics)
	    printf("Can't run %s without graphics\n", functions[count2].name);
	  else
	    {
	      functions[count2].run = 1;
	      testCount += 1;
	      break;
	    }
	}

  if (!testCount)
    {
      printf("\nNo (valid) tests specified.  ");
      usage(argv[0]);
      return (-1);
    }

  printf("\n");
  errors = run();

  if (errors)
    {
      printf("\n%d TESTS FAILED\n", errors);
      return (-1);
    }
  else
    {
      printf("\nALL TESTS PASSED\n");
      return (0);
    }
}
