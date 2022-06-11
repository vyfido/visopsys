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
//  kernelFilesystem.c
//

// This file contains the routines designed to control file system
// objects.

#include "kernelFilesystem.h"
#include "kernelFile.h"
#include "kernelFilesystemTypeFat.h"
#include "kernelText.h"
#include "kernelMultitasker.h"
#include "kernelResourceManager.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>


static kernelFilesystem kernelFilesystemArray[MAX_FILESYSTEMS];
static kernelFilesystem *filesystemPointerArray[MAX_FILESYSTEMS];
static int filesystemCounter = 0;
static kernelFilesystemDriver kernelFilesystemDriverArray[MAX_FILESYSTEMS];
static kernelFilesystemDriver *driverPointerArray[MAX_FILESYSTEMS];
static int driverCounter = 0;

static int filesystemIdCounter = 1;
static int filesystemManagerInitialized = 0;


static void kernelFilesystemSyncd(void)
{
  // This function will be a new thread spawned by the filesystem manager
  // that synchronizes filesystems as a low-priority process.  

  // Wait 10 seconds to let the system get moving
  kernelMultitaskerWait(200);

  while(1)
    {
      // Let "sync" do all filesystems
      kernelFilesystemSync(NULL);

      // Yield the rest of the timeslice and wait for at least 10 seconds
      kernelMultitaskerWait(200);
    }
}


static kernelFileSysTypeEnum detectType(const kernelDiskObject *theDisk)
{
  // This function takes a disk object (initialized, with its driver
  // accounted for) and calls functions to determine its type.  At the
  // moment there will be a set number of known filesystem types that
  // will be more-or-less hard-coded into this routine.  Of course this
  // isn't desirable and should/will be fixed to be more flexible in 
  // the future.  The function returns an enumeration value reflecting
  // the type it found (including possibly "unknown").  This 
  // function should not be called by a user.  It should really only 
  // be called by the installDriver function.

  kernelFileSysTypeEnum filesystemType = unknown;
  char *typeName = NULL;


  int (*detectFat) (const kernelDiskObject *) = FATDETECT;

  // We will assume that the detection routines being called will do
  // all of the necessary checking of the kernelDiskObject before using
  // it.  Since we're not actually using it here, we won't duplicate
  // that work.

  // Check 'em

  // Check for FAT
  if (detectFat(theDisk) == 1)
    {
      filesystemType = Fat;
      typeName = "FAT";
    }
  else
    {
      filesystemType = unknown;
      typeName = "unsupported";
    }

  kernelLog("%s filesystem found on disk %d", typeName, theDisk->diskNumber);

  return (filesystemType);
}


static kernelFileSysTypeEnum installDriver(kernelFilesystem *theFilesystem)
{
  // This function is called to make sure that the correct filesystem
  // driver is installed in the filesystem object.  It is passed a
  // pointer to that filesystem object, and a pointer to an empty
  // driver object.  It sets them up with the correct values.  This 
  // function should not be called by a user.  It should really only 
  // be called by the kernelFilesystemNew function.

  kernelFilesystemDriver *theDriver = NULL;
  kernelFileSysTypeEnum theType = unknown;


  // Now, we have make sure the filesystem's disk object has been properly
  // installed
  if (theFilesystem->disk == NULL)
    {
      // Make an error
      kernelError(kernel_error, NULL_FS_DISK_OBJECT);
      return (unknown);
    }


  // OK, now call the routine that checks the types
  theType = detectType(theFilesystem->disk);


  if (theType == unknown)
    {
      // Oops.  We don't know the filesystem type, so we have to make
      // an error and return a bad status.
      kernelError(kernel_error, UNKNOWN_FS_TYPE);
      return (unknown);
    }


  // Create the new driver object
  theDriver = driverPointerArray[driverCounter++];

  // Initialize a filesystem driver object.
  kernelMemClear(theDriver, sizeof(kernelFilesystemDriver));
  
  // kernelTextPrint("Making new driver in position ");
  // kernelTextPrintInteger(driverCounter - 1);
  // kernelTextPrint(", dr P is ");
  // kernelTextPrintInteger((unsigned int) theDriver);
  // kernelTextNewline();
  
  // Now, we use the type to install the driver
  
  if (theType == Fat)
    {
      // We will install the FAT driver
      theDriver->driverType = Fat;
      theDriver->driverTypeName = "FAT";

      theDriver->driverDetect = FATDETECT;
      theDriver->driverCheck = FATCHECK;
      theDriver->driverMount = FATMOUNT;
      theDriver->driverSync = FATSYNC;
      theDriver->driverUnmount = FATUNMOUNT;
      theDriver->driverGetFree = FATGETFREE;
      theDriver->driverNewEntry = FATNEWENTRY;
      theDriver->driverInactiveEntry = FATINACTIVE;
      theDriver->driverReadFile = FATREADFILE;
      theDriver->driverWriteFile = FATWRITEFILE;
      theDriver->driverCreateFile = FATCREATEFILE;
      theDriver->driverDeleteFile = FATDELETEFILE;
      theDriver->driverFileMoved = FATFILEMOVED;
      theDriver->driverReadDir = FATREADDIR;
      theDriver->driverWriteDir = FATWRITEDIR;
      theDriver->driverMakeDir = FATMAKEDIR;
      theDriver->driverRemoveDir = FATREMOVEDIR;
      theDriver->driverTimestamp = FATTIMESTAMP;
    }

  else
    {
      // Ack!  This is some new enumeration type that we've not been
      // trained for!
      kernelError(kernel_error, UNKNOWN_FS_TYPE);
      return (unknown);
    }

  // Attach this driver to the filesystem object
  theFilesystem->filesystemDriver = theDriver;

  return (theType);
}


static int checkObjectAndDriver(kernelFilesystem *theFilesystem, 
				char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;


  // Make sure the filesystem object isn't NULL (which could indicate that the
  // filesystem has not been properly mounted)
  if (theFilesystem == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_FS_OBJECT);
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the filesystem has a non-NULL driver
  if (theFilesystem->filesystemDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_FS_DRIVER);
      return (status = ERR_NOSUCHDRIVER);
    }

  // Make sure the disk object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (theFilesystem->disk == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_FS_DISK_OBJECT);
      return (status = ERR_NULLPARAMETER);
    }

  return (status = 0);
}


static kernelFilesystem *filesystemFromPath(const char *path)
{
  // The filesystem functions can use this function to determine which
  // filesystem is being requested based on the mount point name

  kernelFilesystem *theFilesystem = NULL;
  int count;


  // Ok, we need to loop through the filesystems that are currently in use.
  // We will use the strncmp function to find one whose name matches
  // the requested one.
  for (count = 0; count < filesystemCounter; count ++)
    {
      // Is this the requested filesystem?
      if (!strcmp((char *) filesystemPointerArray[count]->mountPoint,
			path))
	{
	  theFilesystem = filesystemPointerArray[count];
	  break;
	}
    }

  // After we get finished with this loop, we simply return "theFilesystem".
  // If no filesystem was a match, this will simply be NULL.
  return (theFilesystem);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFilesystemInitialize(void)
{
  // This function just does the small amount of initialization needed
  // to manage the filesystems
  
  int status = 0;
  int syncerPID = 0;
  int count;

  
  // Reset the counters 
  filesystemCounter = 0;
  driverCounter = 0;

  for (count = 0; count < MAX_FILESYSTEMS; count ++)
    {
      filesystemPointerArray[count] = &kernelFilesystemArray[count];
      driverPointerArray[count] = &kernelFilesystemDriverArray[count];
    }

  // Initialize the file entry manager
  status = kernelFileInitialize();
  
  if (status < 0)
    {
      kernelError(kernel_error, "Error initializing file manager");
      return (status);
    }

  // Spawn the filesystem synchronizer
  syncerPID = kernelMultitaskerSpawn(kernelFilesystemSyncd, 
		     "filesystem synchronizer", 0 /* no args */, NULL);

  // Make sure we were successful
  if (syncerPID < 0)
    {
      // Make a kernelError
      kernelError(kernel_error, NOSYNCD_MESG);
      return (syncerPID);
    }

  // Re-nice the synchronizer
  status = kernelMultitaskerSetProcessPriority(syncerPID, 
					       (PRIORITY_LEVELS - 2));
  
  if (status < 0)
    {
      // Oops, we couldn't make it low-priority.  This is probably
      // bad, but not fatal.  Make a kernelError.
      kernelError(kernel_warn, BADSYNCDPRIORITY_MESG);
    }

  // We're initialized
  filesystemManagerInitialized = 1;

  return (status = 0);
}


int kernelFilesystemSync(const char *path)
{
  // This function synchronizes the requested filesystem (or, alternatively,
  // all filesystems) to disk.  It will ensure that all of the dirty
  // directories are written, and call the applicable filesystem driver(s')
  // "sync" function to cause the driver to update all its private structures
  // (if any) to the physical disk as well.  Returns 0 on success, negative
  // otherwise

  int status = 0;
  int errors = 0;
  int doFilesystems = 0;
  int doneFilesystems = 0;
  char fileName[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;


  // Do NOT do anything until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (status = ERR_NOTINITIALIZED);
    }

  // If we are synchronizing a particular filesystem, write all of the
  // dirty directories starting at its mount point.  Otherwise, write all
  // dirty directories starting at the root.
  if (path != NULL)
    {
      status = kernelFileFixupPath(path, fileName);
      
      if (status < 0)
	{
	  kernelError(kernel_error, "Error doing fixup of path");
	  return (status);
	}
    }
  else
    strncpy(fileName, "/", 1);
  
  // Call the function to write all the dirty directories to the disks,
  // starting at the appropriate point.
  status = kernelFileWriteDirtyDirs(fileName);

  if (status < 0)
    {
      // We won't quit because of this; we will still call the 'sync' routine
      // for each applicable filesystem.  All dirty directories that were
      // written successfully will then be saved.
      kernelError(kernel_warn, "Unable to write all dirty directories");
      errors++;
    }

  // If we are doing all the filesystems, we will need to keep track
  // of how many we're doing and which number we're at
  if (path != NULL)
    doFilesystems = 1;
  else
    doFilesystems = filesystemCounter;
  doneFilesystems = 0;

  for ( ; doneFilesystems < doFilesystems; doneFilesystems++ )
    {

      // If "path" is not NULL, that means we are synchronizing a particular
      // filesystem (such as when we unmount).
      if (path != NULL)
	{
	  // Find the filesystem object based on its name
	  theFilesystem = filesystemFromPath(fileName);
	}
      else
	{
	  // Grab the next filesystem from the list.
	  theFilesystem = filesystemPointerArray[doneFilesystems];
	}

      // Check the filesystem pointer
      if (theFilesystem == NULL)
	{
	  kernelError(kernel_error, 
		      "Couldn't get a pointer to the filesystem structure");
	  // Don't quit; just proceed to the next filesystem
	  errors++;
	  continue;
	}

      theDriver = (kernelFilesystemDriver *) theFilesystem->filesystemDriver;

      // (Redundantly check the filesystem) its driver, and its disk object
      status = checkObjectAndDriver(theFilesystem, __FUNCTION__);

      if (status < 0)
	{
	  // Don't quit; just proceed to the next filesystem
	  errors++;
	  continue;
	}

      // If the filesystem driver has no 'sync' function, we should skip
      // to the next filesystem, if any
      if (theDriver->driverSync == NULL)
	continue;

      // OK, the filesystem has a 'sync' function: we should call it.

      // For any given filesystem, only one "sync" operation should be 
      // occurring at any given time.  Obtain a "sync lock"
      status = kernelResourceManagerLock(&(theFilesystem->syncLock));
	  
      if (status < 0)
	{
	  kernelError(kernel_error,
		      "Couldn't obtain synchronization lock on filesystem");
	  // Don't quit; just proceed to the next filesystem
	  errors++;
	  continue;
	}

      status = theDriver->driverSync(theFilesystem);
      
      // Release the sync lock
      kernelResourceManagerUnlock(&(theFilesystem->syncLock));

      if (status < 0)
	{
	  kernelError(kernel_error,
		      "Filesystem driver's sync operation was unsuccessful");
	  // Don't quit; just proceed to the next filesystem
	  errors++;
	  continue;
	}

      // Loop to the next filesystem, if applicable.
    }

  // If there were errors synchronizing some filesystem(s), we should return
  // an error code of some kind

  if (errors)
    return (status = ERR_IO);
  else
    // Return success.
    return (status = 0);
}


int kernelFilesystemMount(int diskNumber, const char *path)
{
  // This function creates and registers (mounts) a new filesystem definition.
  // If successful, it returns the filesystem number of the filesystem it
  // mounted.  Otherwise it returns negative.

  int status = 0;
  char mountPoint[MAX_PATH_LENGTH];
  char parentDirName[MAX_PATH_LENGTH];
  char mountDirName[MAX_NAME_LENGTH];
  kernelDiskObject *theDisk = NULL;
  kernelFilesystem *parentFilesystem = NULL;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *parentDir = NULL;
  kernelFileSysTypeEnum theType = unknown;
  int count;


  // Do NOT mount any filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the requested mount point name isn't NULL
  if (path == NULL)
    return (status = ERR_NULLPARAMETER);

  theDisk = kernelFindDiskObjectByNumber(diskNumber);

  // Make sure it exists
  if (theDisk == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_FS_DISK_OBJECT);
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, mountPoint);
  if (status < 0)
    return (status);

  kernelLog("Mounting %s filesystem on disk %d", mountPoint,
	    theDisk->diskNumber);

  // Make sure there aren't already too many filesystems mounted
  if (filesystemCounter >= MAX_FILESYSTEMS)
    {
      // Make the error
      kernelError(kernel_error, MAX_FS_EXCEEDED);
      return (status = ERR_NOFREE);
    }

  // Make sure that the filesystem hasn't already been mounted (or
  // more precisely, that the requested mount point is not already
  // in use).  Also make sure that the disk object has not already been 
  // mounted as some other name.
  for (count = 0; count < filesystemCounter; count ++)
    if ((!strcmp((char *) filesystemPointerArray[count]->mountPoint, 
		       mountPoint)) ||
	(theDisk == filesystemPointerArray[count]->disk))
      {
	// Make the error
	kernelError(kernel_error, FS_ALREADY_MOUNTED);
	return (status = ERR_ALREADY);
      }

  // If this is NOT the root filesystem we're mounting, we need to make
  // sure that the mount point doesn't already exist.  This is because
  // The root directory of the new filesystem will be inserted into its
  // parent directory here.

  if (strcmp(mountPoint, "/") != 0)
    {
      // The root filesystem will be the parent filesystem
      parentFilesystem = filesystemFromPath("/");

      if (parentFilesystem == NULL)
	{
	  kernelError(kernel_error, "Unable to look up the root filesystem");
	  return (status = ERR_BADADDRESS);
	}

      // Make sure the mount point doesn't currently exist
      if (kernelFileLookup(mountPoint) != NULL)
	{
	  kernelError(kernel_error, "The mount point already exists.");
	  return (status = ERR_ALREADY);
	}

      // Make sure the parent directory of the mount point DOES exist.
      status = 
	kernelFileSeparateLast(mountPoint, parentDirName, mountDirName);

      if (status < 0)
	{
	  kernelError(kernel_error, "Bad path to mount point");
	  return (status);
	}

      parentDir = kernelFileLookup(parentDirName);

      if (parentDir == NULL)
	{
	  kernelError(kernel_error, 
		      "Mount point parent directory doesn't exist");
	  return (status = ERR_NOCREATE);
	}
    }
  
  // Get a new filesystem object from the list
  theFilesystem = filesystemPointerArray[filesystemCounter];

  // Initialize the filesystem object
  kernelMemClear((void *) theFilesystem, sizeof(kernelFilesystem));

  // Fill in the information that we already know for this filesystem

  // Set the filesystem's Id number
  theFilesystem->filesystemNumber = filesystemIdCounter;

  // Make "mountPoint" be the filesystem's mount point
  strcpy((char *) theFilesystem->mountPoint, mountPoint);

  // Make "theDisk" be the filesystem's disk object
  theFilesystem->disk = theDisk;
  
  // Now install the filesystem driver functions
  theType = installDriver(theFilesystem);
  
  // Make sure it was successful
  if (theType == unknown)
    {
      // We don't need to make an error, since this will already have 
      // been done by the installDriver routine.
      return (status = ERR_INVALID);
    }

  theDriver = (kernelFilesystemDriver *) theFilesystem->filesystemDriver;

  // If the driver has a 'check' function, run it now.  The function
  // can make up its own mind whether the filesystem really needs to be
  // checked or not.
  if (theDriver->driverCheck != NULL)
    {
      status = theDriver->driverCheck(theFilesystem->disk);

      if (status < 0)
	{
	  // Make the error
	  kernelError(kernel_error, "Filesystem consistency check failed");
	  return (status);
	}
    }

  // Make sure the driver's mounting routine is not NULL
  if (theDriver->driverMount == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_FS_DRIVER);
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Mount the filesystem
  status = theDriver->driverMount(theFilesystem);

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, FS_DRIVER_INIT_FAILED);
      // Return the code that the driver routine produced
      return (status);
    }

  if (strcmp(mountPoint, "/") == 0)
    {
      // Set the root filesystem pointer
      status = kernelFileSetRoot(theFilesystem->filesystemRoot);

      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to set root directory");
	  return (status);
	}
    }
  else
    {
      // If this is not the root filesystem, insert the filesystem's
      // root directory into the file entry tree.

      // Set the name of the mount point directory
      strcpy((char *) theFilesystem->filesystemRoot->fileName, 
		   mountDirName);

      status = 
	kernelFileInsertEntry(theFilesystem->filesystemRoot, parentDir);

      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to insert directory");
	  return (status);
	}
    }

  // Looks like we were successful.  Increment the filesystem counter 
  // and Id counter
  filesystemCounter += 1;
  filesystemIdCounter+= 1;

  // kernelTextPrintInteger(filesystemCounter);
  // kernelTextPrintLine(" filesystems mounted");

  return (theFilesystem->filesystemNumber);
}


int kernelFilesystemUnmount(const char *path)
{
  // This routine will remove a filesystem object and its driver from the 
  // lists.  It takes the filesystem number and returns the new number of 
  // filesystems in the array.  If the filesystem number doesn't exist, 
  // it returns negative

  int status = 0;
  char mountPointName[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *mountPoint = NULL;
  void *temp = NULL;
  int numberFilesystems = -1;
  int pointerArrayPosition = 0;
  int count;


  // Do NOT unmount any filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name isn't NULL
  if (path == NULL)
    return (status = ERR_NULLPARAMETER);

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, mountPointName);
  if (status < 0)
    return (status);

  // Find the filesystem object based on its name
  theFilesystem = filesystemFromPath(mountPointName);

  // If the filesystem is NULL, then we assume the requested mount
  // does not exist, and we can make a useful error message
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "No such filesystem mounted");
      return (status = ERR_NOSUCHENTRY);
    }

  theDriver = (kernelFilesystemDriver *) theFilesystem->filesystemDriver;

  // (Redundantly check the filesystem) its driver, and its disk object
  status = 
    checkObjectAndDriver(theFilesystem, __FUNCTION__);

  if (status < 0)
    return (status);

  // kernelTextPrint("Unmounting ");
  // kernelTextPrintLine(theFilesystem->mountPoint);
  
  // DO NOT attempt to unmount the root filesystem if there are
  // ANY other filesystems mounted.  This would be bad, since root is
  // the parent filesystem of all other filesystems
  if (strcmp((char *) theFilesystem->mountPoint, "/") == 0)
    if (filesystemCounter > 1)
      {
	// Make the error
	kernelError(kernel_error, FS_ROOT_ORPHANS);
	return (status = ERR_BUSY);
      }

  // Synchronize the filesystem
  status = kernelFilesystemSync((char *) theFilesystem->mountPoint);

  // If the sync wasn't successful, we shouldn't unmount the filesystem
  // I guess.  This might result in data loss
  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, NO_SYNCHY_NO_UMOUNT);
      return (status);
    }

  // Get the file entry for the mount point
  mountPoint = kernelFileLookup(mountPointName);

  if (mountPoint == NULL)
    {
      kernelError(kernel_error, "Unable to locate the mount point entry");
      return (status = ERR_NOSUCHDIR);
    }

  // Starting at the mount point, unbuffer all of the filesystem's files
  // from the file entry tree
  status = kernelFileUnbufferRecursive(mountPoint);

  if (status < 0)
    {
      kernelError(kernel_error, "Unable to unbuffer filesystem files");
      return (status);
    }

  // Do a couple of additional things if this is not the root directory
  if (strcmp(mountPointName, "/") != 0)
    {
      // Remove the mount point's file entry from its parent directory
      status = kernelFileRemoveEntry(mountPoint);
      
      if (status < 0)
	// Just warn about this; it's not fatal.
	kernelError(kernel_warn, "Unable to remove mount point entry");

      // Release the mount point entry
      kernelFileReleaseEntry(mountPoint);
    }

  // If the driver's unmount routine is not NULL, call it
  if (theDriver->driverUnmount != NULL)
    theDriver->driverUnmount(theFilesystem);

  // It doesn't matter whether the shutdown call was "successful".
  // If it wasn't, there's really nothing we can do about it from
  // here.

  // We have to remove that filesystem by shifting all of the following
  // objects in the array up by one spot and reduce the counter that keeps
  // track of the number of devices.  If the device was the last spot,
  // all we do is reduce the counter.

  // Find the position of the filesystem's pointer in the pointer array
  for (pointerArrayPosition = 0; 
       ((filesystemPointerArray[pointerArrayPosition] != theFilesystem &&
	 (pointerArrayPosition < filesystemCounter)));
	pointerArrayPosition++);
  // Empty loop body is deliberate

  // What if we didn't find it?
  if (pointerArrayPosition >= filesystemCounter)
    {
      // We didn't find the filesystem in the pointer list!
      kernelError(kernel_error, MISSING_FS_OBJECT);
      return (status = ERR_NOSUCHENTRY);
    }
  
  for (count = pointerArrayPosition; count < (filesystemCounter - 1); 
       count ++)
    filesystemPointerArray[count] = filesystemPointerArray[count + 1];

  // Move the removed one to the end if there was more than 1
  if (filesystemCounter > 1)
    filesystemPointerArray[filesystemCounter - 1] = theFilesystem;

  // Now do the same thing for the driver objects

  temp = (void *) driverPointerArray[pointerArrayPosition];

  for (count = pointerArrayPosition; count < (filesystemCounter - 1); 
       count ++)
    driverPointerArray[count] = driverPointerArray[count + 1];

  if (driverCounter > 1)
    // Move the removed one to the end if there was more than 1
    driverPointerArray[filesystemCounter - 1] = 
      (kernelFilesystemDriver *) temp;

  // Reduce the counters
  filesystemCounter -= 1;
  driverCounter -= 1;  

  // kernelTextPrintInteger(filesystemCounter);
  // kernelTextPrintLine(" filesystems mounted");
  
  return (numberFilesystems = filesystemCounter);
}


int kernelFilesystemUnmountAll(void)
{
  // This routine will unmount all mounted filesystems, including the root
  // filesystem.  Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFilesystem *fs = NULL;
  kernelFilesystem *root = NULL;
  int errors = 0;
  int count;


  // Do NOT unmount any filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (status = ERR_NOTINITIALIZED);
    }  

  // We will loop through all of the mounted filesystems, unmounting
  // each of them (except root) until only root remains.  Finally, we
  // unmount the root also.

  count = 0;

  for (count = 0; count < filesystemCounter; count ++)
    {
      fs = &kernelFilesystemArray[count];

      if (!strcmp((char *) fs->mountPoint, "/"))
	{
	  root = fs;
	  continue;
	}

      // Unmount this filesystem
      status = kernelFilesystemUnmount((char *) fs->mountPoint);

      // Success?
      if (status < 0)
	{
	  // Don't quit, just make an error message
	  kernelError(kernel_warn, "Unable to unmount filesystem");
	  errors++;
	  continue;
	}
      else
	{
	  // Decrement the counter, since this filesystem will have been
	  // replaced with another if any more exist
	  count--;
	}
    }

  // Don't attempt to unmount root if there are still other filesystems
  // mounted
  if (filesystemCounter == 1)
    {
      // 'root' should have been set whenever we encountered it in the
      // previous loop
      if (root == NULL)
	{
	  // Ack!  We didn't find root in the list?
	  kernelError(kernel_warn, "Unable to identify the root filesystem");
	  errors++;
	}

      // Now unmount the root filesystem
      status = kernelFilesystemUnmount("/");
      
      // Success?
      if (status < 0)
	{
	  // Don't quit, just make an error message
	  kernelError(kernel_warn, "Unable to unmount root filesystem");
	  errors++;
	}
    }

  // If there were any errors, we should return an error code of some kind
  if (errors)
    return (status = ERR_INVALID);
  else
    // Return success
    return (status = 0);
}


int kernelFilesystemNumberMounted(void)
{
  // This function will return the number of filesystems currently
  // mounted

  int status = 0;


  // Do not report filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (status = ERR_NOTINITIALIZED);
    }  

  return (filesystemCounter);
}


void kernelFilesystemFirstFilesystem(char *fsName)
{
  // This function will return the filesystem name of the first
  // filesystem in the list.  Returns NULL in the first character
  // of the filesystem name if there are no filesystems mounted.


  // Do not look for filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      fsName[0] = NULL;
      return;
    }  

  // Make sure the filesystem name buffer we have been passed is not NULL
  if (fsName == NULL)
    return;

  if (filesystemCounter == 0)
    // There are NO filesystems mounted
    fsName[0] = NULL;
  else
    // Return the appropriate filesystem name
    strcpy(fsName, (char *) filesystemPointerArray[0]->mountPoint);

  return;
}


void kernelFilesystemNextFilesystem(char *fsName)
{
  // This function will find the indicated filesytem in the filesystem
  // list, and return the one that follows it.  Returns a NULL as the
  // first name character if there are no more filesystems

  int status = 0;
  char mountPoint[MAX_PATH_LENGTH];
  int count;


  // Do not look for filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      fsName[0] = NULL;
      return;
    }  

  // Make sure the previous name buffer isn't NULL
  if (fsName == NULL)
    return;

  // Are there ANY filesystems mounted?
  if (filesystemCounter <= 1)
    {
      fsName[0] = NULL;
      return;
    }

  // Initialize the buffer we will use for holding the "official" mount point
  mountPoint[0] = NULL;

  // Fix up the path of the mount point, get the "official" version
  status = kernelFileFixupPath(fsName, mountPoint);
  if (status < 0)
    return;

  // Scan through the list of filesystems for the previous one
  for (count = 0; count < filesystemCounter; count ++)
    if (strcmp((char *) filesystemPointerArray[count]->mountPoint, 
		     mountPoint) == 0)
      break;

  // Did we find it?  Is there at least one more?
  if (count < (filesystemCounter - 1))
    strcpy(fsName, (char *) filesystemPointerArray[count + 1]->mountPoint);
  else
    fsName[0] = NULL;

  return;
}


unsigned int kernelFilesystemGetFree(const char *path)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  unsigned int freeSpace = 0;
  char mountPoint[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;


  // Do NOT look at any filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (freeSpace = 0);
    }  

  // Make sure the path name isn't NULL
  if (path == NULL)
    return (freeSpace = 0);

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, mountPoint);
  if (status < 0)
    return (freeSpace = 0);

  // Find the filesystem object based on its name
  theFilesystem = filesystemFromPath(mountPoint);

  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "Unable to determine filesystem from path");
      return (freeSpace = 0);
    }

  theDriver = (kernelFilesystemDriver *) theFilesystem->filesystemDriver;

  // Check the filesystem, its driver, and its disk object
  status = checkObjectAndDriver(theFilesystem, __FUNCTION__);
  if (status < 0)
    // Report NO free space
    return (freeSpace = 0);
  
  // OK, we just have to check on the filsystem driver function we want
  // to call
  if (theDriver->driverGetFree == NULL)
    {
      // Make the error
      kernelError(kernel_error, NULL_FS_DRIVER_FUNCTION);
      // Report NO free space
      return (freeSpace = 0);
    }

  // Lastly, we can call our target function
  freeSpace = theDriver->driverGetFree(theFilesystem);

  // Return the same value as the driver function.
  return (freeSpace);
}


unsigned int kernelFilesystemGetBlockSize(const char *path)
{
  // This function simply returns the block size of the filesystem
  // that contains the specified path.

  int status = 0;
  unsigned int blockSize = 0;
  char fixedPath[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;


  // Do NOT look at any filesystems until we have been initialized
  if (filesystemManagerInitialized == 0)
    {
      // Make the error
      kernelError(kernel_error, FS_NOT_INITIALIZED);
      return (blockSize = 0);
    }  

  // Make sure the path name isn't NULL
  if (path == NULL)
    return (blockSize = 0);

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, fixedPath);
  if (status < 0)
    return (blockSize = 0);

  // Find the filesystem object based on its name
  theFilesystem = filesystemFromPath(fixedPath);

  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "Unable to determine filesystem from path");
      return (blockSize = 0);
    }

  // Grab the block size
  blockSize = theFilesystem->blockSize;

  return (blockSize);
}
