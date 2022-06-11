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
//  kernelFilesystem.c
//

// This file contains the routines designed to manage file systems

#include "kernelFilesystem.h"
#include "kernelFile.h"
#include "kernelDriverManagement.h"
#include "kernelMultitasker.h"
#include "kernelLock.h"
#include "kernelSysTimer.h"
#include "kernelMalloc.h"
#include "kernelMiscFunctions.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>

static kernelFilesystem *filesystemPointerArray[MAX_FILESYSTEMS];
static int filesystemCounter = 0;


static kernelFilesystemDriver *detectType(kernelDisk *theDisk)
{
  // This function takes a disk structure (initialized, with its driver
  // accounted for) and calls functions to determine its type.  At the
  // moment there will be a set number of known filesystem types that
  // will be more-or-less hard-coded into this routine.  Of course this
  // isn't desirable and should/will be fixed to be more flexible in 
  // the future.  The function returns an enumeration value reflecting
  // the type it found (including possibly "unknown").  This 
  // function should not be called by a user.  It should really only 
  // be called by the installDriver function.

  int status = 0;
  kernelFilesystemDriver *tmpDriver = NULL;
  kernelFilesystemDriver *driver = NULL;

  // We will assume that the detection routines being called will do
  // all of the necessary checking of the kernelDisk before using
  // it.  Since we're not actually using it here, we won't duplicate
  // that work.

  // If it's a CD-ROM, only check ISO
  if ((((kernelPhysicalDisk *) theDisk->physical)->type == idecdrom) ||
      (((kernelPhysicalDisk *) theDisk->physical)->type == scsicdrom))
    {
      tmpDriver = kernelDriverGetIso();
      status = tmpDriver->driverDetect(theDisk);
      if (status == 1)
	{
	  driver = tmpDriver;
	  goto finished;
	}
    }
  else
    {
      // Check for FAT
      tmpDriver = kernelDriverGetFat();
      status = tmpDriver->driverDetect(theDisk);
      if (status == 1)
	{
	  driver = tmpDriver;
	  goto finished;
	}

      // Check for EXT
      tmpDriver = kernelDriverGetExt();
      status = tmpDriver->driverDetect(theDisk);
      if (status == 1)
	{
	  driver = tmpDriver;
	  goto finished;
	}
    }
  
 finished:

  if (driver)
    {
      if (!strcmp((char *) theDisk->fsType, "unknown"))
	// Copy this filesystem type name into the disk structure.  The
	// filesystem driver can change it if desired.
	strncpy((char *) theDisk->fsType, driver->driverTypeName,
		FSTYPE_MAX_NAMELENGTH);

      // Set the operation flags based on which filesystem functions are
      // non-NULL.
      theDisk->opFlags = 0;
      if (driver->driverFormat)
	theDisk->opFlags |= FS_OP_FORMAT;
      if (driver->driverCheck)
	theDisk->opFlags |= FS_OP_CHECK;
      if (driver->driverDefragment)
	theDisk->opFlags |= FS_OP_DEFRAG;
    }

  return (driver);
}


static int installDriver(kernelFilesystem *theFilesystem)
{
  // This function is called to make sure that the correct filesystem
  // driver is installed in the filesystem structure.  It is passed a
  // pointer to that filesystem structure, and a pointer to an empty
  // driver structure.  It sets them up with the correct values.  This 
  // function should not be called by a user.  It should really only 
  // be called by the kernelFilesystemNew function.

  int status = 0;
  kernelFilesystemDriver *theDriver = NULL;

  // Now, we have make sure the filesystem's disk structure has been properly
  // installed
  if (theFilesystem->disk == NULL)
    {
      kernelError(kernel_error, "The filesystem structure has a NULL disk "
		  "structure");
      return (status = ERR_NULLPARAMETER);
    }

  // OK, now call the routine that checks the types
  theDriver = detectType((kernelDisk *) theFilesystem->disk);
  if (theDriver == NULL)
    {
      // Oops.  We don't know the filesystem type, so we have to make
      // an error and return a bad status.
      kernelError(kernel_error, "The system was not able to determine the "
		  "type of this filesystem");
      return (status = ERR_INVALID);
    }

  theFilesystem->driver = theDriver;

  return (status = 0);
}


static kernelFilesystem *getNewFilesystem(kernelDisk *theDisk)
{
  // Get a new filesystem structure from the list, fill in some values,
  // detect the filesystem type, and install the driver.

  int status = 0;
  kernelFilesystem *theFilesystem = NULL;

  // Get a new filesystem structure
  theFilesystem = kernelMalloc(sizeof(kernelFilesystem));

  // Make "theDisk" be the filesystem's disk structure
  theFilesystem->disk = theDisk;
  
  // Now install the filesystem driver functions
  status = installDriver(theFilesystem);
  if (status < 0)
    {
      kernelFree((void *) theFilesystem);
      return (theFilesystem = NULL);
    }

  filesystemPointerArray[filesystemCounter++] = theFilesystem;

  // All set
  return (theFilesystem);
}


static void releaseFilesystem(kernelFilesystem *theFilesystem)
{
  // Release the filesystem.  First, find it in our list

  int count;

  for (count = 0; count < filesystemCounter; count ++)
    {
      if (filesystemPointerArray[count] == theFilesystem)
	{
	  if ((filesystemCounter > 1) && (count < (filesystemCounter - 1)))
	    filesystemPointerArray[count] =
	      filesystemPointerArray[filesystemCounter - 1];

	  kernelFree((void *) theFilesystem);

	  filesystemCounter -= 1;
	}
    }

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


int kernelFilesystemScan(kernelDisk *scanDisk)
{
  // Scan a logical disk and see if we can determine the filesystem type

  // Scan a disk to determine its filesystem type, etc.
  if (detectType(scanDisk))
    return (0);
  else
    return (ERR_INVALID);
}


int kernelFilesystemFormat(const char *diskName, const char *type,
			   const char *label, int longFormat)
{
  // This function is a wrapper for the filesystem driver's 'format' function,
  // if applicable.

  int status = 0;
  kernelDisk *theDisk = NULL;
  kernelFilesystemDriver *theDriver = NULL;
 
  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  theDisk = kernelGetDiskByName(diskName);
  // Make sure it exists
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "No such disk \"%s\"", diskName);
      return (status = ERR_NULLPARAMETER);
    }

  // Get a temporary filesystem driver to use for formatting
  if (!strncasecmp(type, "fat", 3))
    theDriver = kernelDriverGetFat();

  if (theDriver == NULL)
    {
      kernelError(kernel_error, "Invalid filesystem type \"%s\" for format!",
		  type);
      return (status = ERR_NOSUCHENTRY);
    }

  // Make sure the driver's checking routine is not NULL
  if (theDriver->driverFormat == NULL)
    {
      kernelError(kernel_error, "The filesystem driver does not support the "
		  "'format' operation");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Create the filesystem
  status = theDriver->driverFormat(theDisk, type, label, longFormat);
  if (status < 0)
    // Return the code that the driver routine produced
    return (status);

  // Get the kernel to re-scan the partition tables
  kernelDiskReadPartitions();

  // Finished
  return (status);
}


int kernelFilesystemCheck(const char *diskName, int force, int repair)
{
  // This function is a wrapper for the filesystem driver's 'check' function,
  // if applicable.

  int status = 0;
  kernelDisk *theDisk = NULL;
  kernelFilesystem *tmpFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
 
  // Check params
  if (diskName == NULL)
    return (status = ERR_NULLPARAMETER);

  theDisk = kernelGetDiskByName(diskName);
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "No such disk \"%s\"", diskName);
      return (status = ERR_NULLPARAMETER);
    }

  // Get a temporary filesystem to use for checking
  tmpFilesystem = getNewFilesystem(theDisk);
  if (tmpFilesystem == NULL)
    return (status = ERR_NOFREE);
  
  theDriver = (kernelFilesystemDriver *) tmpFilesystem->driver;
  // Make sure the driver's checking routine is not NULL
  if (theDriver->driverCheck == NULL)
    {
      kernelError(kernel_error, "The filesystem driver does not support the "
		  "'check' operation");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Check the filesystem
  status = theDriver->driverCheck(tmpFilesystem, force, repair);
  if (status < 0)
    // Return the code that the driver routine produced
    return (status);

  // Release the temporary filesystem structure we allocated
  releaseFilesystem(tmpFilesystem);

  // Finished
  return (status);
}


int kernelFilesystemDefragment(const char *diskName)
{
  // This function is a wrapper for the filesystem driver's 'defragment'
  // function, if applicable.
  
  int status = 0;
  kernelDisk *theDisk = NULL;
  kernelFilesystem *tmpFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;

  theDisk = kernelGetDiskByName(diskName);
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "No such disk \"%s\"", diskName);
      return (status = ERR_NULLPARAMETER);
    }

  // Get a temporary filesystem to use for defragging
  tmpFilesystem = getNewFilesystem(theDisk);
  if (tmpFilesystem == NULL)
    {
      kernelError(kernel_error, "Unable to allocate a temporary filesystem "
		  "structure for defragmenting");
      return (status = ERR_NOFREE);
    }
  
  theDriver = (kernelFilesystemDriver *) tmpFilesystem->driver;

  // Make sure the driver's checking routine is not NULL
  if (theDriver->driverCheck == NULL)
    {
      kernelError(kernel_error, "The filesystem driver does not support the "
		  "'defragment' operation");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Defrag the filesystem
  status = theDriver->driverDefragment(tmpFilesystem);
  if (status < 0)
    // Return the code that the driver routine produced
    return (status);

  // Release the temporary filesystem structure we allocated
  releaseFilesystem(tmpFilesystem);

  // Finished
  return (status);
}


int kernelFilesystemMount(const char *diskName, const char *path)
{
  // This function creates and registers (mounts) a new filesystem definition.
  // If successful, it returns the filesystem number of the filesystem it
  // mounted.  Otherwise it returns negative.

  int status = 0;
  char mountPoint[MAX_PATH_LENGTH];
  char parentDirName[MAX_PATH_LENGTH];
  char mountDirName[MAX_NAME_LENGTH];
  kernelDisk *theDisk = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelFilesystem *parentFilesystem = NULL;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *parentDir = NULL;
  int count;

  // Check params
  if ((diskName == NULL) || (path == NULL))
    return (status = ERR_NULLPARAMETER);

  theDisk = kernelGetDiskByName(diskName);
  if (theDisk == NULL)
    {
      kernelError(kernel_error, "No such disk \"%s\"", diskName);
      return (status = ERR_NULLPARAMETER);
    }
  physicalDisk = (kernelPhysicalDisk *) theDisk->physical;

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, mountPoint);
  if (status < 0)
    return (status);

  kernelLog("Mounting %s filesystem on disk %s", mountPoint, theDisk->name);

  // Make sure that the filesystem hasn't already been mounted (or
  // more precisely, that the requested mount point is not already
  // in use).  Also make sure that the disk structure has not already been 
  // mounted as some other name.
  for (count = 0; count < filesystemCounter; count ++)
    if ((!strcmp((char *) filesystemPointerArray[count]->mountPoint, 
		       mountPoint)) ||
	(theDisk == filesystemPointerArray[count]->disk))
      {
	kernelError(kernel_error, "The requested disk or mount point is "
		    "already in use by the system");
	return (status = ERR_ALREADY);
      }

  // If this is NOT the root filesystem we're mounting, we need to make
  // sure that the mount point doesn't already exist.  This is because
  // The root directory of the new filesystem will be inserted into its
  // parent directory here.  This is un-UNIXy.

  if (strcmp(mountPoint, "/"))
    {
      // The root filesystem will be the parent filesystem
      parentFilesystem = filesystemFromPath("/");
      if (parentFilesystem == NULL)
	{
	  kernelError(kernel_error, "Unable to look up the root filesystem");
	  return (status = ERR_BADADDRESS);
	}

      // Make sure the mount point doesn't currently exist
      if (kernelFileLookup(mountPoint))
	{
	  kernelError(kernel_error, "The mount point already exists.");
	  return (status = ERR_ALREADY);
	}

      // Make sure the parent directory of the mount point DOES exist.
      status = kernelFileSeparateLast(mountPoint, parentDirName, mountDirName);
      if (status < 0)
	{
	  kernelError(kernel_error, "Bad path to mount point");
	  return (status);
	}

      parentDir = kernelFileLookup(parentDirName);
      if (parentDir == NULL)
	{
	  kernelError(kernel_error, "Mount point parent directory doesn't "
		      "exist");
	  return (status = ERR_NOCREATE);
	}
    }

  theFilesystem = getNewFilesystem(theDisk);
  if (theFilesystem == NULL)
    // Probably unable to determine the type of the filesystem
    return (status = ERR_NOSUCHDRIVER);
  
  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // Make sure the driver's mounting routine is not NULL
  if (theDriver->driverMount == NULL)
    {
      kernelError(kernel_error, "The filesystem driver does not support the "
		  "'mount' operation");
      return (status = ERR_NOSUCHFUNCTION);
    }
  
  // Fill in any information that we already know for this filesystem

  // Make "mountPoint" be the filesystem's mount point
  strcpy((char *) theFilesystem->mountPoint, mountPoint);

  // Get a new file entry for the filesystem's root directory
  theFilesystem->filesystemRoot = kernelFileNewEntry(theFilesystem);
  if (theFilesystem->filesystemRoot == NULL)
    // Not enough free file structures
    return (status = ERR_NOFREE);
  
  theFilesystem->filesystemRoot->type = dirT;
  theFilesystem->filesystemRoot->filesystem = (void *) theFilesystem;

  if (!strcmp(mountPoint, "/"))
    {
      // The root directory has no parent
      theFilesystem->filesystemRoot->parentDirectory = NULL;

      // Set the root filesystem pointer
      status = kernelFileSetRoot(theFilesystem->filesystemRoot);
      if (status < 0)
	return (status);
    }
  else
    {
      // If this is not the root filesystem, insert the filesystem's
      // root directory into the file entry tree.
      status = kernelFileInsertEntry(theFilesystem->filesystemRoot, parentDir);
      if (status < 0)
	return (status);
    }

  // Mount the filesystem
  status = theDriver->driverMount(theFilesystem);
  if (status < 0)
    {
      /*
      // If the driver has a 'check' function, run it now.  The function can
      // make up its own mind whether the filesystem really needs to be checked
      // or not.
      if (theDriver->driverCheck)
	{
	  // No force, no repair
	  status = theDriver->driverCheck(theFilesystem, 0, // no force
					  0); // no repair
	  if (status < 0)
	    {
	      // The filesystem may contain errors.  Before we fail the whole
	      // operation, ask whether the user wants to try and repair it.
	      kernelTextPrint("The filesystem may contain errors.\nDo you "
			      "want to try to repair it? (y/n): ");
	      kernelTextInputGetc(&yesNo);
	      kernelTextNewline();

	      if ((yesNo == 'y') || (yesNo == 'Y'))
		// Force, repair
		status = theDriver->driverCheck(theFilesystem, 1, // force
						1); // repair
	      if (status < 0)
		{
		  kernelError(kernel_error, "Filesystem consistency check "
			      "failed.  Mount aborted.");
		  return (status);
		}
	    }
	}
      else
      */
	{      
	  return (status);
	}
    }

  // Set the name of the mount point directory
  if (!strcmp(mountPoint, "/"))
    strcpy((char *) theFilesystem->filesystemRoot->name, "/");
  else
    strcpy((char *) theFilesystem->filesystemRoot->name, mountDirName);

  // If the disk is removable and has a 'lock' function, lock it
  if ((physicalDisk->fixedRemovable == removable) &&
      (physicalDisk->driver->driverSetLockState))
    physicalDisk->driver->driverSetLockState(physicalDisk->deviceNumber, 1);

  // If the driver specified that the filesystem is read-only, mark the disk
  // as read-only also
  if (theFilesystem->readOnly)
    physicalDisk->readOnly = 1;

  return (status = 0);
}


int kernelFilesystemUnmount(const char *path)
{
  // This routine will remove a filesystem structure and its driver from the 
  // lists.  It takes the filesystem number and returns the new number of 
  // filesystems in the array.  If the filesystem number doesn't exist, 
  // it returns negative

  int status = 0;
  char mountPointName[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *mountPoint = NULL;
  kernelPhysicalDisk *physicalDisk = NULL;
  int numberFilesystems = -1;

  // Make sure the path name isn't NULL
  if (path == NULL)
    return (status = ERR_NULLPARAMETER);

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, mountPointName);
  if (status < 0)
    return (status);

  // Find the filesystem structure based on its name
  theFilesystem = filesystemFromPath(mountPointName);
  if (theFilesystem == NULL)
    {
      // If the filesystem is NULL, then we assume the requested mount
      // does not exist, and we can make a useful error message
      kernelError(kernel_error, "No such filesystem '%s' mounted",
		  mountPointName);
      return (status = ERR_NOSUCHENTRY);
    }

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // DO NOT attempt to unmount the root filesystem if there are
  // ANY other filesystems mounted.  This would be bad, since root is
  // the parent filesystem of all other filesystems
  if (strcmp((char *) theFilesystem->mountPoint, "/") == 0)
    if (filesystemCounter > 1)
      {
	kernelError(kernel_error, "Cannot unmount / when child filesystems "
		    "are still mounted");
	return (status = ERR_BUSY);
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
    return (status);

  // Do a couple of additional things if this is not the root directory
  if (strcmp(mountPointName, "/") != 0)
    {
      // Remove the mount point's file entry from its parent directory
      kernelFileRemoveEntry(mountPoint);

      // Release the mount point entry
      kernelFileReleaseEntry(mountPoint);
    }

  // If the driver's unmount routine is not NULL, call it
  if (theDriver->driverUnmount)
    theDriver->driverUnmount(theFilesystem);

  // It doesn't matter whether the unmount call was "successful".  If it
  // wasn't, there's really nothing we can do about it from here.

  // Sync the disk cache
  status = kernelDiskSyncDisk((char *) theFilesystem->disk->name);
  if (status < 0)
    kernelError(kernel_warn, "Unable to sync disk \"%s\" after unmount",
		theFilesystem->disk->name);

  physicalDisk = (kernelPhysicalDisk *) theFilesystem->disk->physical;

  // If this is a removable disk, invalidate the disk cache
  if (physicalDisk->fixedRemovable == removable)
    {
      status = kernelDiskInvalidateCache((char *) ((kernelPhysicalDisk *)
				   theFilesystem->disk->physical)->name);
      if (status < 0)
	kernelError(kernel_warn, "Unable to invalidate \"%s\" disk cache "
		    "before mount", theFilesystem->disk->name);

      // If it has an 'unlock' function, unlock it
      if (physicalDisk->driver->driverSetLockState)
	physicalDisk->driver->driverSetLockState(physicalDisk->deviceNumber,
						 0);
    }

  releaseFilesystem(theFilesystem);
  
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

  // We will loop through all of the mounted filesystems, unmounting
  // each of them (except root) until only root remains.  Finally, we
  // unmount the root also.

  count = 0;

  for (count = 0; count < filesystemCounter; count ++)
    {
      fs = filesystemPointerArray[count];

      if (!strcmp((char *) fs->mountPoint, "/"))
	{
	  root = fs;
	  continue;
	}

      // Unmount this filesystem
      status = kernelFilesystemUnmount((char *) fs->mountPoint);
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
	  errors++;
	}

      // Now unmount the root filesystem
      status = kernelFilesystemUnmount("/");
      if (status < 0)
	{
	  // Don't quit, just make an error message
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


kernelFilesystem *kernelFilesystemGet(char *fsName)
{
  // This function will return the named filesystem.

  kernelFilesystem *theFilesystem = NULL;
  int count;

  // Make sure the filesystem name buffer we have been passed is not NULL
  if (fsName == NULL)
    return (theFilesystem = NULL);

  for (count = 0; count < filesystemCounter; count ++)
    if (!strcmp((char *) filesystemPointerArray[count]->mountPoint, fsName))
      {
	theFilesystem = filesystemPointerArray[count];
	break;
      }

  return (theFilesystem);
}


unsigned kernelFilesystemGetFree(const char *path)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  unsigned freeSpace = 0;
  char mountPoint[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;

  // Make sure the path name isn't NULL
  if (path == NULL)
    return (freeSpace = 0);

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, mountPoint);
  if (status < 0)
    return (freeSpace = 0);

  // Find the filesystem structure based on its name
  theFilesystem = filesystemFromPath(mountPoint);
  if (theFilesystem == NULL)
    return (freeSpace = 0);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // OK, we just have to check on the filsystem driver function we want
  // to call
  if (theDriver->driverGetFree == NULL)
    {
      kernelError(kernel_error, "The filesystem driver does not support the"
		  "'getFree' operation");
      // Report NO free space
      return (freeSpace = 0);
    }

  // Lastly, we can call our target function
  freeSpace = theDriver->driverGetFree(theFilesystem);

  // Return the same value as the driver function.
  return (freeSpace);
}


unsigned kernelFilesystemGetBlockSize(const char *path)
{
  // This function simply returns the block size of the filesystem
  // that contains the specified path.

  int status = 0;
  unsigned blockSize = 0;
  char fixedPath[MAX_PATH_LENGTH];
  kernelFilesystem *theFilesystem = NULL;

  // Make sure the path name isn't NULL
  if (path == NULL)
    return (blockSize = 0);

  // Fix up the path of the mount point
  status = kernelFileFixupPath(path, fixedPath);
  if (status < 0)
    return (blockSize = 0);

  // Find the filesystem structure based on its name
  theFilesystem = filesystemFromPath(fixedPath);
  if (theFilesystem == NULL)
    return (blockSize = 0);

  // Grab the block size
  blockSize = theFilesystem->blockSize;

  return (blockSize);
}
