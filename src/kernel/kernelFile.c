//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelFile.c
//

// This file contains the routines designed for managing the file
// system tree.

#include "kernelFile.h"
#include "kernelMalloc.h"
#include "kernelLock.h"
#include "kernelSysTimer.h"
#include "kernelRtc.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>
#include <string.h>

#include "kernelText.h"
 

// The root directory
static kernelFileEntry *rootDirectory = NULL;

// These array pointers represent arrays of unordered, cross-linked lists 
// of files and directories, respectively.  Basically, the filesystem 
// manager uses these lists to "remember" files and directories it has 
// encountered, therefore saving time when filesystem requests are processed.
// A management routine will keep track of the things in the arrays, and 
// will remove the least-recently used file/directory trees as needed when
// the arrays become too full.
static kernelFileEntry *fileEntryMemory = NULL;
static kernelFileEntry *fileEntries[MAX_BUFFERED_FILES];
static unsigned usedFileEntries;

static int initialized = 0;

  
static int initFileEntryList(void)
{
  // This function is used to initialize (or re-initialize) the list
  // of used/free files/directories.

  int status = 0;
  int count;

  // Allocate memory for file entries
  fileEntryMemory = kernelMalloc(sizeof(kernelFileEntry) * MAX_BUFFERED_FILES);
  if (fileEntryMemory == NULL)
    {
      kernelError(kernel_error, 
		  "Error allocating memory for file entry lists");
      return (status = ERR_MEMORY);
    }

  // Initialize all of the kernelFileEntry structures.

  for (count = 0; count < MAX_BUFFERED_FILES; count ++)
    fileEntries[count] = &fileEntryMemory[count];

  // Reset the number of used files and directories to 0
  usedFileEntries = 0;

  return (status = 0);
}


static inline int isLeafDir(kernelFileEntry *dir)
{
  // This function will determine whether the supplied directory entry
  // is a 'leaf' directory.  A leaf directory is defined as a directory
  // which contains no other subdirectories with buffered contents.
  // Returns 1 if the directory is a leaf, 0 otherwise.

  int status = 0;
  kernelFileEntry *listItemPointer = NULL;

  listItemPointer = dir->contents;
  
  while (listItemPointer != NULL)
    {
      if ((listItemPointer->type == dirT) &&
	  (listItemPointer->contents != NULL))
	break;
      else
	listItemPointer = 
	  (kernelFileEntry *) listItemPointer->nextEntry;
    }

  if (listItemPointer == NULL)
    // It is a leaf directory
    return (status = 1);
  else
    // It's not a leaf directory
    return (status = 0);
}


static int unbufferDirectory(kernelFileEntry *dir)
{
  // This function is internal, and is called when the tree of file
  // and directory entries becomes too full.  It will "un-buffer" all of
  // one directory entry's contents (sub-entries) from memory.  
  // The decision about which entry to un-buffer is based on LRU (least
  // recently used).  Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFileEntry *listItemPointer = NULL;
  kernelFileEntry *nextListItem = NULL;

  // We should have a directory that is safe to unbuffer.  We can return
  // this directory's contents (sub-entries) to the list of free entries.

  listItemPointer = (kernelFileEntry *) dir->contents;

  // Step through the list of directory entries
  while (listItemPointer != NULL)
    {
      nextListItem = listItemPointer->nextEntry;
      kernelFileReleaseEntry(listItemPointer);
      listItemPointer = nextListItem;
    }

  dir->contents = NULL;

  // This directory now looks to the system as if it had not yet been read
  // from disk.

  // Return success
  return (status = 0);
}


static kernelFileEntry *findUnbufferableDir(void)
{
  // This function is internal, and is used to find a directory that
  // can be unbuffered from the file entry tree to make space for other
  // entries.  It does this by looking for the least recently used "leaf"
  // directory that has no open files.  It will give preference to directories
  // that are not "dirty" (so that they don't need to be written back to the
  // disk before unbuffering).  Returns a pointer to the directory on
  // success, NULL on failure.

  kernelFileEntry *oldestDirectory = NULL;
  kernelFileEntry *listItemPointer = NULL;
  unsigned oldestDirectoryAge = 0xFFFFFFFF;
  int count;

  // Ok, we will loop through all of the used kernelFileEntry structures.
  // Look for the oldest directory.
  
  for (count = 0; count < usedFileEntries; count ++)
    {
      // Skip '.' and '..' entries
      if (!strcmp((char *) fileEntries[count]->name, ".") ||
	  !strcmp((char *) fileEntries[count]->name, ".."))
	continue;
	    
      // Don't bother with anything that isn't a directory OR is a
      // directory with no contents.
      if ((fileEntries[count]->type != dirT) ||
	  (fileEntries[count]->contents == NULL))
	continue;

      // Don't unbuffer any directories that are the root of filesystems
      if (fileEntries[count] == ((kernelFilesystem *) 
			 fileEntries[count]->filesystem)->filesystemRoot)
	continue;

      // Now, is this directory the least recently used?
      if (fileEntries[count]->lastAccess >= oldestDirectoryAge)
	continue;

      // We only want to un-buffer the contents of a "leaf" directory.
      // Make SURE this one is a leaf so we don't orphan any memory
      if (!isLeafDir(&fileEntryMemory[count]))
	continue;

      // Finally, loop through the files in the directory and make sure
      // that none of them are currently open.
      listItemPointer = fileEntryMemory[count].contents;
      while (listItemPointer != NULL)
	{
	  if (listItemPointer->openCount)
	    break;

	  else if (kernelLockVerify(&(listItemPointer->lock)))
	    break;

	  else
	    listItemPointer = 
	      (kernelFileEntry *) listItemPointer->nextEntry;
	}

      if (listItemPointer != NULL)
	// There are open files here
	continue;

      // Otherwise, we have a new oldest directory
      oldestDirectory = fileEntries[count];
      oldestDirectoryAge = oldestDirectory->lastAccess;
    }

  // If we found something, it will be in the variable.
  return (oldestDirectory);
}


static inline void fileEntry2File(kernelFileEntry *fileEntry, file *theFile)
{
  // This little function will copy the applicable parts from a file
  // entry structure to an external 'file' structure.

  strncpy(theFile->name, (char *) fileEntry->name, MAX_NAME_LENGTH);
  theFile->name[MAX_NAME_LENGTH - 1] = '\0';

  // Resolve links
  if (fileEntry->type == linkT)
    fileEntry = kernelFileResolveLink(fileEntry);

  theFile->handle = (void *) fileEntry;
  theFile->type = fileEntry->type;

  strncpy(theFile->filesystem,
	  (char *) ((kernelFilesystem *) fileEntry->filesystem)
	  ->mountPoint, MAX_PATH_LENGTH);
  theFile->filesystem[MAX_PATH_LENGTH - 1] = '\0';

  theFile->creationTime = fileEntry->creationTime;
  theFile->creationDate = fileEntry->creationDate;
  theFile->accessedTime = fileEntry->accessedTime;
  theFile->accessedDate = fileEntry->accessedDate;
  theFile->modifiedTime = fileEntry->modifiedTime;
  theFile->modifiedDate = fileEntry->modifiedDate;
  theFile->size = fileEntry->size;
  theFile->blocks = fileEntry->blocks;
  theFile->blockSize = 
    ((kernelFilesystem *) fileEntry->filesystem)->blockSize;

  return;
}


static int dirIsEmpty(kernelFileEntry *theDir)
{
  // This function is internal, and is used to determine whether there
  // are any files in a directory (aside from the non-entries '.' and
  // '..'.  Returns 1 if the directory is empty.  Returns 0 if the
  // directory contains entries.  Returns negative on error.

  kernelFileEntry *listItemPointer = NULL;
  int isEmpty = 0;

  // Make sure the directory is really a directory
  if (theDir->type != dirT)
    {
      kernelError(kernel_error, "Directory to check is not a directory");
      return (isEmpty = ERR_NOTADIR);
    }

  // Assign the first item in the directory to our iterator pointer
  listItemPointer = theDir->contents;

  while(listItemPointer != NULL)
    {
      if (!strcmp((char *) listItemPointer->name, ".") ||
	  !strcmp((char *) listItemPointer->name, ".."))
	listItemPointer = (kernelFileEntry *) listItemPointer->nextEntry;
      else
	return (isEmpty = 0);
    }

  return (isEmpty = 1);
}


static int isDescendent(kernelFileEntry *leaf, kernelFileEntry *node)
{
  // This function is internal, and can be used to determine whether 
  // the "leaf" entry is a descendent of the "node" entry.  This is 
  // important to check during move and copy operations.  Returns 1 if
  // true (is a descendent), 0 if false, and negative on error

  kernelFileEntry *listItemPointer = NULL;
  int status = 0;

  // Make sure "node" and "leaf" aren't NULL
  if (node == NULL)
    {
      kernelError(kernel_error, "NULL directory entry");
      return (status = ERR_NULLPARAMETER);
    }

  if (leaf == NULL)
    {
      kernelError(kernel_error, "NULL directory entry");
      return (status = ERR_NULLPARAMETER);
    }

  // If "node" is not a directory, then it obviously has no descendents.
  // Return false
  if (node->type != dirT)
    {
      kernelError(kernel_error, "Node is not a directory");
      return (status = 0);
    }

  // If "leaf" and "node" are the same, return true
  if (node == leaf)
    return (status = 1);

  listItemPointer = leaf;

  // Do a loop to step upward from the "leaf" through its respective
  // ancestor directories.  If the parent is NULL at any point, we return
  // false.  If the parent ever equals "node", return true.
  while(1)
    {
      listItemPointer = listItemPointer->parentDirectory;

      if (listItemPointer == NULL)
	// It is not a descendant
	return (status = 0);

      else if (listItemPointer == node)
	// It is a descendant
	return (status = 1);
    }
}


static inline void updateCreationTime(kernelFileEntry *entry)
{
  // This will update the creation date and time of a file entry.
  
  entry->creationDate = kernelRtcPackedDate();
  entry->creationTime = kernelRtcPackedTime();
  entry->lastAccess = kernelSysTimerRead();
  return;
}


static inline void updateModifiedTime(kernelFileEntry *entry)
{
  // This will update the modified date and time of a file entry.
  
  entry->modifiedDate = kernelRtcPackedDate();
  entry->modifiedTime = kernelRtcPackedTime();
  entry->lastAccess = kernelSysTimerRead();
  return;
}


static inline void updateAccessedTime(kernelFileEntry *entry)
{
  // This will update the accessed date and time of a file entry.
  
  entry->accessedDate = kernelRtcPackedDate();
  entry->accessedTime = kernelRtcPackedTime();
  entry->lastAccess = kernelSysTimerRead();
  return;
}


static inline void updateAllTimes(kernelFileEntry *entry)
{
  // This will update all the dates and times of a file entry.
  
  entry->creationDate = kernelRtcPackedDate();
  entry->creationTime = kernelRtcPackedTime();
  entry->modifiedDate = entry->creationDate;
  entry->modifiedTime = entry->creationTime;
  entry->accessedDate = entry->creationDate;
  entry->accessedTime = entry->creationTime;
  entry->lastAccess = kernelSysTimerRead();
  return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelFileInitialize(void)
{
  // This function just does the small amount of initialization needed
  // to manage the files
  
  int status = 0;
  
  // Initialize the file entry list
  status = initFileEntryList();
  if (status < 0)
    {
      kernelError(kernel_error, "Error making file entry list");
      return (status);
    }

  // We're initialized
  initialized = 1;

  return (status = 0);
}


int kernelFileSetRoot(kernelFileEntry *rootDir)
{
  // This just sets the root filesystem pointer
  
  int status = 0;

  // Make sure 'rootDir' isn't NULL
  if (rootDir == NULL)
    {
      kernelError(kernel_error, "Root directory entry is NULL");
      return (status = ERR_NOTINITIALIZED);
    }
    
  // Assign it to the variable
  rootDirectory = rootDir;

  // Return success
  return (status = 0);
}


int kernelFileFixupPath(const char *originalPath, char *newPath)
{
  // This function will take a path string and convert it to
  // an absolute path, removing any unneccessary characters 
  // and resolving any '.' '..' or '...' components to their real
  // targets.  Returns 0 on success, negative otherwise.

  int status = 0;
  int originalLength = 0;
  int newPathLength = 0;
  int count;

  // Check the string pointers and make sure they're not NULL
  if ((originalPath == NULL) || (newPath == NULL))
    {
      kernelError(kernel_error, "Character string pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure there is a leading '/' or '\'
  if ((originalPath[0] != '/') && (originalPath[0] != '\\'))
    {
      kernelError(kernel_error, "Path to fix is not an absolute pathname");
      return (status = ERR_INVALID);
    }

  originalLength = strlen(originalPath);

  // OK, we step through the original path, dealing with the 
  // various possibilities
  for (count = 0; count < originalLength; count ++)
    {
      // Deal with slashes
      if ((originalPath[count] == '/') || (originalPath[count] == '\\'))
	{
	  if ((newPathLength > 0) && (newPath[newPathLength - 1] == '/'))
	    continue;
	  else
	    {
	      newPath[newPathLength++] = '/';
	      continue;
	    }
	}

      // Deal with '.' and '..' between separators
      if ((originalPath[count] == '.') && (newPath[newPathLength - 1] == '/'))
	{
	  // We must determine whether this will be dot or dotdot.  If it
	  // is dot, we simply remove this path level.  If it is dotdot,
	  // we have to remove the previous path level
	  if ((originalPath[count + 1] == '/') || 
	      (originalPath[count + 1] == '\\') ||
	      (originalPath[count + 1] == NULL))
	    {
	      // It's only dot.  Skip this one level
	      count += 1;
	      continue;
	    }
	  else if ((originalPath[count + 1] == '.') && 
		   ((originalPath[count + 2] == '/') || 
		    (originalPath[count + 2] == '\\') ||
		    (originalPath[count + 2] == NULL)))
	    {
	      // It's dotdot.  We must skip backward in the new path 
	      // until the next-but-one separator.  If we're at the
	      // root level, simply copy (it will probably fail later
	      // as a 'no such file')
	      if (newPathLength > 1)
		{
		  newPathLength -= 1;
		  while((newPath[newPathLength - 1] != '/') && 
			(newPathLength > 1))
		    newPathLength -= 1;
		}
	      else
		{
		  newPath[newPathLength++] = originalPath[count];
		  newPath[newPathLength++] = originalPath[count + 1];
		  newPath[newPathLength++] = originalPath[count + 2];
		}
	      count += 2;
	      continue;
	    }
	}

      // Other possibilities, just copy
      newPath[newPathLength++] = originalPath[count];
    }

  // Stick the NULL on the end
  newPath[newPathLength] = NULL;

  // Return success;
  return (status = 0);
}


kernelFileEntry *kernelFileNewEntry(kernelFilesystem *theFilesystem)
{
  // This function will find an unused file entry and return it to the
  // calling function.

  int status = 0;
  kernelFileEntry *oldestDirectory = NULL;
  kernelFileEntry *theEntry = NULL;
  kernelFilesystemDriver *theDriver = NULL;

  // Make sure the filesystem argument is not NULL
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem argument");
      return (theEntry = NULL);
    }

  // Make sure there is a free file entry available
  while (usedFileEntries >= MAX_BUFFERED_FILES)
    {
      // Try to find the oldest directory suitable for unbuffering
      oldestDirectory = findUnbufferableDir();

      // Make sure we found something
      if (oldestDirectory == NULL)
	{
	  // We couldn't find a directory to unbuffer
	  kernelError(kernel_error,
		      "No unbufferable directories.  No free file entries.");
	  return (theEntry = NULL);
	}

      status = unbufferDirectory(oldestDirectory);
      if (status < 0)
	{
	  // Oops, error unbuffering the directory
	  kernelError(kernel_error,
		      "Couldn't unbuffer directory.  No free file entries.");
	  return (theEntry = NULL);
	}
    }

  // Get a free file entry.  Grab it from the spot right after the
  // last used file entry
  theEntry = fileEntries[usedFileEntries++];

  // Make sure it's not NULL
  if (theEntry == NULL)
    {
      kernelError(kernel_error, "No free file entries");
      return (theEntry = NULL);
    }
      
  // Initialize this new one
  kernelMemClear((void *) theEntry, sizeof(kernelFileEntry));

  // Set some default time/date values
  updateAllTimes(theEntry);

  // We need to call the appropriate filesystem so that it can attach
  // its private data structure to the file

  // Make note of the assigned filesystem in the entry
  theEntry->filesystem = (void *) theFilesystem;

  // Get a pointer to the filesystem driver
  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // OK, we just have to call the filesystem driver function
  if (theDriver->driverNewEntry != NULL)
    theDriver->driverNewEntry(theEntry);

  return (theEntry);
}


void kernelFileReleaseEntry(kernelFileEntry *theEntry)
{
  // This function takes a file entry to release, and puts it back
  // in the pool of free file entries.

  int spotNumber = -1;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *temp = NULL;
  int count;

  // Make sure the supplied entry is not NULL
  if (theEntry == NULL)
    return;

  // If there's some private filesystem data attached to the file entry
  // structure, we will need to release it.
  if (theEntry->fileEntryData != NULL)
    {
      // Get the filesystem pointer from the file entry structure
      theFilesystem = theEntry->filesystem;

      if (theFilesystem != NULL)
	{
	  // Get a pointer to the filesystem driver
	  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

	  // OK, we just have to check on the filesystem driver function
	  if (theDriver->driverInactiveEntry != NULL)
	    theDriver->driverInactiveEntry(theEntry);
	}
    }

  // Find the supplied entry in the list.
  for (count = 0; ((count < usedFileEntries) &&
		   (count < MAX_BUFFERED_FILES)); count ++)
    if (theEntry == fileEntries[count])
      {
	spotNumber = count;
	break;
      }

  // Did we find it?
  if (spotNumber == -1)
    return;

  // Put the entry back into the pool of unallocated entries.  The way
  // we do this is to move the LAST entry into the spot occupied by this
  // entry, and stick this entry where the old LAST entry was.  The only
  // time we DON'T do this is if the entry was (a) the only used entry; or
  // (b) the last used entry.

  // Update the counts
  usedFileEntries -= 1;

  if ((usedFileEntries != 0) && (spotNumber != usedFileEntries))
    {
      temp = fileEntries[usedFileEntries];
      fileEntries[usedFileEntries] = theEntry;
      fileEntries[spotNumber] = temp;
    }

  // Cool.
  return;
}


int kernelFileInsertEntry(kernelFileEntry *theFile, kernelFileEntry *directory)
{
  // This function is used to add a new entry to a target directory.  The
  // function will verify that the file does not already exist.

  int status = 0;
  int numberFiles = 0;
  kernelFileEntry *listItemPointer = NULL;
  kernelFileEntry *previousItemPointer = NULL;

  // Make sure that neither of the pointers we're receiving are NULL
  if ((directory == NULL) || (theFile == NULL))
    {
      kernelError(kernel_error, "File or directory entry is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure directory is really a directory
  if (directory->type != dirT)
    {
      kernelError(kernel_error, "Directory in which to insert file is not "
		  "a directory");
      return (status = ERR_NOTADIR);
    }

  // Make sure the entry does not already exist
  listItemPointer = (kernelFileEntry *) directory->contents;
  while (listItemPointer != NULL)
    {
      // We do a case-sensitive comparison here, regardless of whether
      // the filesystem driver cares about case.  We are worried about
      // exact matches.
      if (!strcmp((char *) listItemPointer->name, (char *) theFile->name))
	{
	  kernelError(kernel_error, "A file by the name \"%s\" already exists "
		      "in the directory \"%s\"", theFile->name,
		      directory->name);
	  return (status = ERR_ALREADY);
	}

      listItemPointer = (kernelFileEntry *) listItemPointer->nextEntry;
    }

  // Make sure that the number of entries in this directory has not
  // exceeded (and is not about to exceed) either the maximum number of
  // legal directory entries
      
  numberFiles = kernelFileCountDirEntries(directory);
      
  if (numberFiles >= MAX_DIRECTORY_ENTRIES)
    {
      // Make an error that the directory is full
      kernelError(kernel_error,
		  "The directory is full; can't create new entry");
      return (status = ERR_NOCREATE);
    }

  // Set the parent directory
  theFile->parentDirectory = (void *) directory;

  // Set the "list item" pointer to the start of the file chain
  listItemPointer = (kernelFileEntry *) directory->contents;
  
  // For each file in the file chain, we loop until we find a
  // filename that is alphabetically greater than our new entry.
  // At that point, we insert our new entry in the previous spot.

  while (listItemPointer != NULL)
    {
      // Make sure we don't scan the '.' or '..' entries, if there are any
      if (!strcmp((char *) listItemPointer->name, ".") ||
	  !strcmp((char *) listItemPointer->name, ".."))
	{
	  // Move to the next filename.
	  previousItemPointer = listItemPointer;
	  listItemPointer = (kernelFileEntry *) listItemPointer->nextEntry;
	  continue;
	}

      if (strcmp((char *) listItemPointer->name, (char *) theFile->name) < 0)
	{
	  // This item is alphabetically "less than" the one we're
	  // inserting.  Move to the next filename.
	  previousItemPointer = listItemPointer;
	  listItemPointer = (kernelFileEntry *) listItemPointer->nextEntry;
	}

      else
	break;
    }

  // When we fall through to this point, there are a few possible
  // scenarios:
  // 1. the filenames we now have (of the new entry and an existing
  //    entry in the list) are identical
  // 2. The list item is alphabetically "after" our new entry.
  // 3. The directory is empty, or we reached the end of the file chain 
  //    without finding anything that's alphabetically "after" our new entry
  
  // If the directory is empty, or we've reached the end of the list, 
  // we should insert the new entry there
  if (listItemPointer == NULL)
    { 
      if (directory->contents == NULL)
	// It's the first file
	directory->contents = (void *) theFile;
      else
	{
	  // It's the last file
	  previousItemPointer->nextEntry = (void *) theFile;
	  theFile->previousEntry = (void *) previousItemPointer;
	}

      // Either way, there's no next entry
      theFile->nextEntry = (void *) NULL;
    }

  // If the filenames are the same it's an error.  We are only worried
  // about exact matches, so do a case-sensitive comparison regardless of
  // whether the filesystem driver cares about case.
  else if (!strcmp((char *) theFile->name, (char *) listItemPointer->name))
    {
      // Oops, the file exists
      kernelError(kernel_error, "A file named '%s' exists in the directory",
		  theFile->name);
      return (status = ERR_ALREADY);
    }

  // Otherwise, listItemPointer points to an entry that should come AFTER
  // our new entry.  We insert our entry into the previous slot.  Watch
  // out just in case we're BECOMING the first item in the list!
  else
    {
      if (previousItemPointer != NULL)
	{
	  previousItemPointer->nextEntry = (void *) theFile;
	  theFile->previousEntry = (void *) previousItemPointer;
	}
      else
	{
	  // We're the new first item in the list
	  directory->contents = (void *) theFile;
	  theFile->previousEntry = NULL;
	}
      
      theFile->nextEntry = (void *) listItemPointer;
      listItemPointer->previousEntry = (void *) theFile;
    }

  // Update the access time on the directory
  directory->lastAccess = kernelSysTimerRead();

  // Don't mark the directory as dirty; that is the responsibility of the
  // caller, since this function is used in building initial directory
  // structures by the filesystem drivers.

  // Return success
  return (status = 0);
}


int kernelFileRemoveEntry(kernelFileEntry *entry)
{
  // This function is internal, and is used to delete an entry from its
  // parent directory.  It DOES NOT deallocate the file object itself.
  // That must be done, if applicable, by the calling routine.

  kernelFileEntry *directory = NULL;
  kernelFileEntry *previousEntry = NULL;
  kernelFileEntry *nextEntry = NULL;
  int status = 0;
  
  // Make sure that the pointer we're receiving is not NULL
  if (entry == NULL)
    {
      kernelError(kernel_error, "File entry parameter is NULL");
      return (status = ERR_NOSUCHFILE);
    }

  // The directory to delete from is the entry's parent directory
  directory = (kernelFileEntry *) entry->parentDirectory;

  if (directory == NULL)
    {
      kernelError(kernel_error, "File entry has a NULL parent directory");
      return (status = ERR_NOSUCHFILE);
    }
  
  // Make sure the parent dir is really a directory
  if (directory->type != dirT)
    {
      kernelError(kernel_error, "Parent directory is not a directory");
      return (status = ERR_NOTADIR);
    }

  // Remove the item from its place in the directory.

  // Get the item's previous and next pointers
  previousEntry = (kernelFileEntry *) entry->previousEntry;
  nextEntry = (kernelFileEntry *) entry->nextEntry;

  // If we're deleting the first file of the directory, we have to 
  // change the parent directory's "first file" pointer (In addition to 
  // the other things we do) to point to the following file, if any.
  if (entry == (kernelFileEntry *) directory->contents)
    directory->contents = (void *) nextEntry;

  // Make this item's 'previous' item refer to this item's 'next' as its
  // own 'next'.  Did that sound bitchy?
  if (previousEntry != NULL)
    previousEntry->nextEntry = (void *) nextEntry;

  if (nextEntry != NULL)
    nextEntry->previousEntry = (void *) previousEntry;

  // Remove references to its position from this entry
  entry->parentDirectory = NULL;
  entry->previousEntry = NULL;
  entry->nextEntry = NULL;

  // Update the access time on the directory
  directory->lastAccess = kernelSysTimerRead();

  // Don't mark the directory as dirty; that is the responsibility of the
  // caller, since this function is used for things like unbuffering
  // directories.

  return (status = 0);
}


int kernelFileMakeDotDirs(kernelFilesystem *filesystem,
			  kernelFileEntry *parentDir, kernelFileEntry *dir)
{
  // This just makes the '.' and '..' links in a new directory

  int status = 0;
  kernelFileEntry *dotDir = NULL;
  kernelFileEntry *dotDotDir = NULL;

  if ((filesystem == NULL) || (parentDir == NULL) || (dir == NULL))
    {
      kernelError(kernel_error, "NULL parameter");
      return (status = ERR_NULLPARAMETER);
    }

  dotDir = kernelFileNewEntry(filesystem);
  if (dotDir == NULL)
    return (status = ERR_NOFREE);

  strncpy((char *) dotDir->name, ".", 2);
  dotDir->type = linkT;
  dotDir->contents = (void *) dir;

  status = kernelFileInsertEntry(dotDir, dir);

  if (parentDir != dir)
    {
      dotDotDir = kernelFileNewEntry(filesystem);

      if (dotDotDir == NULL)
	{
	  kernelFileReleaseEntry(dotDir);
	  return (status = ERR_NOFREE);
	}

      strncpy((char *) dotDotDir->name, "..", 3);
      dotDotDir->type = linkT;
      dotDotDir->contents = dir->parentDirectory;

      status = kernelFileInsertEntry(dotDotDir, dir);
    }
  
  return (status);
}


int kernelFileCountDirEntries(kernelFileEntry *theDirectory)
{
  // This function is used to count the current number of directory entries
  // in a given directory.  On success it returns the number of entries.
  // Returns negative on error

  int fileCount = 0;
  kernelFileEntry *listItemPointer = NULL;

  // Make sure the directory entry is non-NULL
  if (theDirectory == NULL)
    {
      kernelError(kernel_error, "Directory to count entries is NULL");
      return (fileCount = ERR_NULLPARAMETER);
    }

  // Make sure the directory is really a directory
  if (theDirectory->type != dirT)
    {
      kernelError(kernel_error,
		  "Directory to count entries is not a directory");
      return (fileCount = ERR_NOTADIR);
    }

  // Assign the first item in the directory to our iterator pointer
  listItemPointer = theDirectory->contents;

  while(listItemPointer != NULL)
    {
      listItemPointer = listItemPointer->nextEntry;
      fileCount += 1;
    }

  return (fileCount);
}


kernelFileEntry *kernelFileResolveLink(kernelFileEntry *entry)
{
  // Take a file entry, and if it is a link, resolve the link.

  if ((entry == NULL) || (entry->type != linkT) || (entry->contents == entry))
    return (entry);
  else
    return (kernelFileResolveLink(entry->contents));
}


kernelFileEntry *kernelFileLookup(const char *path)
{
  // This function is called by other subroutines to resolve
  // pathnames and files to fatEntry structures.  On success, it returns
  // the fatEntry of the deepest item of the path it was given.  The
  // target path can resolve either to a subdirectory or a file.  An
  // additional feature is that it will "compress" file path/names, so
  // that redundant portions of the path are removed.  On failure, it returns
  // NULL.

  int status = 0;
  int found = 0;
  char itemName[MAX_PATH_NAME_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *listItem = NULL;
  int count, count2, pathLength;

  // We step through the directory structure, looking for the appropriate
  // directories based on the path we were given.  The string must start
  // with a slash, but may or may not end with a slash.

  if ((path[0] != '/') && (path[0] != '\\'))
    {
      kernelError(kernel_error, "Lookup path must start at root");
      return (listItem = NULL);
    }

  if (path[1] == NULL)
    // The root directory is being requested
    return (listItem = rootDirectory);

  // Start with the root directory
  listItem = rootDirectory;

  pathLength = strlen(path);

  for (count = 1; count <= pathLength; count ++)
    {
      count2 = 0;
      itemName[0] = NULL;

      // Remove any leading slashes now
      while (((path[count] == '/') || (path[count] == '\\')) 
	     && (path[count] != NULL) && (count <= pathLength))
	count ++;

      // Now extract the relevent file name
      while ((path[count] != '/') && (path[count] != '\\') 
	     && (path[count] != NULL) && (count <= pathLength))
	// We add the character to the item name we're constructing
	itemName[count2++] = path[count++];	  

      // Place a NULL at the end of the item name
      itemName[count2] = NULL;

      // Make sure there's actually some content here
      if (!strlen(itemName))
	continue;

      // Otherwise, we have somthing we can work with.

      // Make sure we're working with a real directory, not a link
      if (listItem->type == linkT)
	listItem = kernelFileResolveLink(listItem);

      // Update the access time on this directory
      listItem->lastAccess = kernelSysTimerRead();

      // Find the first item in the "current" directory
      if (listItem->contents != NULL)
	listItem = (kernelFileEntry *) listItem->contents;
      else
	// Nothing in the directory
	return (listItem = NULL);

      while(1)
	{
	  // First, try a case-sensitive comparison, whether or not the
	  // filesystem is case-sensitive.  If that fails and the filesystem
	  // is case-insensitive, try that kind of comparison also.
	  if ((!strcmp((char *) listItem->name, itemName)) ||
	      ((((kernelFilesystem *) 
		 listItem->filesystem)->caseInsensitive) &&
	       !strcasecmp((char *) listItem->name, itemName)))
	    {
	      // Found it.
	      found = 1;
	      break;
	    }

	  if (listItem->nextEntry == NULL)
	    {
	      // Not found
	      found = 0;
	      break;
	    }
	  
	  // Move to the next Item
	  listItem = (kernelFileEntry *) listItem->nextEntry;
	}

      if (!found)
	{
	  // Not found
	  return (listItem = NULL);
	}

      // If this is a link, use the target of the link instead
      if (listItem->type == linkT)
	listItem = kernelFileResolveLink(listItem);

      // Determine whether the requested item is really a directory, and if
      // so, whether the directory's files have been read
      if ((listItem->type == dirT) && (listItem->contents == NULL))
	{
	  // We have to read this directory from the disk.  Allocate a new
	  // directory buffer based on the number of bytes per sector and the
	  // number of sectors used by the directory
	  
	  // Get the filesystem object based on the filesystem number in the
	  // file entry structure
	  theFilesystem = listItem->filesystem;
	  if (theFilesystem == NULL)
	    {
	      kernelError(kernel_error, "Entry has a NULL filesystem pointer");
	      return (listItem = NULL);
	    }

	  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

	  // OK, we just have to check on the filesystem driver function
	  // we want to call
	  if (theDriver->driverReadDir == NULL)
	    {
	      kernelError(kernel_error, "The requested filesystem operation "
			  "is not supported");
	      return (listItem = NULL);
	    }

	  // Increase the open count on the directory's entry while we're
	  // reading it.  This will prevent the filesystem manager from
	  // trying to unbuffer it while we're working
	  listItem->openCount++;

	  // Lastly, we can call our target function
	  status = theDriver->driverReadDir(listItem);
  
	  listItem->openCount--;

	  if (status < 0)
	    {
	      kernelError(kernel_error, "Unable to read directory \"%s\"",
			  listItem->name);
	      return (listItem = NULL);
	    }
	}
    }

  // Update the internal "last access" time on this target item
  listItem->lastAccess = kernelSysTimerRead();

  return (listItem);
}


int kernelFileSeparateLast(const char *combined, char *pathName, 
			   char *fileName)
{
  // This function will take a combined pathname/filename string and
  // separate the two.  The user will pass in the "combined" string
  // along with two pre-allocated char arrays to hold the resulting
  // separated elements.

  int status = 0;
  int count, combinedLength;

  combinedLength = strlen(combined);

  // Make sure there's something there
  if (combinedLength <= 0)
    return (status = ERR_NOSUCHFILE);

  // Make sure we're not exceeding the MicrosoftTM(C) defined limit
  // for the combined path and filename
  if (combinedLength >= MAX_PATH_NAME_LENGTH)
    return (status = ERR_INVALID);

  // Initialize the fileName and pathName strings
  fileName[0] = NULL;
  pathName[0] = NULL;

  // Make sure there's at least one (i.e. a leading '/' or '\' character)
  if ((combined[0] != '/') && (combined[0] != '\\'))
    {
      kernelError(kernel_error,
		  "File path to separate is not an absolute path");
      return (status = ERR_INVALID);
    }

  // Discard any trailing separators.  Make sure we don't discard the
  // leading separator if that's what it is.  If there is only
  // the leading separator, we don't need to go any farther
  while (((combined[combinedLength - 1] == '/') || 
	  (combined[combinedLength - 1] == '\\')) && 
	 (combinedLength > 1))
    combinedLength -= 1;

  if (combinedLength == 1)
    {
      strncpy(pathName, "/", 2);
      return (status = 0);
    }

  // Do a loop backwards from the end of the combined until we hit
  // a(nother) '/' or '\' character.  Of course, that might be the '/' 
  // or '\' of root directory fame.  We know we'll hit at least one.
  for (count = (combinedLength - 1); 
       ((count >= 0) && (combined[count] != '/') &&
	(combined[count] != '\\')); count --);
  // (empty loop body is deliberate)
  
  // Now, count points at the offset of the last '/' or '\' character before
  // the final component of the path name

  // Make sure neither the path or the combined exceed the maximum
  // lengths
  if ((count > MAX_PATH_LENGTH) || 
      ((combinedLength - count) > MAX_NAME_LENGTH))
    {
      kernelError(kernel_error, "File path exceeds maximum length");
      return (status = ERR_INVALID);
    }

  // Copy everything before count into the path string.  We skip the
  // trailing '/' or '\' unless it's the first character
  if (count == 0)
    {
      pathName[0] = combined[0];
      pathName[1] = '\0';
    }
  else
    {
      strncpy(pathName, combined, count);
      pathName[count] = '\0';
    }

  // Copy everything after it into the name string
  strncpy(fileName, (combined + (count + 1)), 
	  (combinedLength - (count + 1)));
  fileName[combinedLength - (count + 1)] = '\0';

  return (status = 0);
}


int kernelFileFirst(const char *path, file *fileStructure)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fixedPath[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *directory = NULL;
  
  // Do not look for files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Directory pathname for lookup is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the file structure we were passed is not NULL
  if (fileStructure == NULL)
    {
      kernelError(kernel_error, "File structure for lookup is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fixedPath);
  if (status < 0)
    {
      kernelError(kernel_error, "Error doing fixup of path");
      return (status);
    }

  // Make the starting directory correspond to the path we were given
  directory = kernelFileLookup(fixedPath);

  // Make sure it's good, and that it's really a subdirectory
  if ((directory == NULL) || (directory->type != dirT))
    {
      kernelError(kernel_error, "Invalid directory for lookup");
      return (status = ERR_NOSUCHFILE);
    }

  fileStructure->handle = (void *) NULL;  // INVALID

  if (directory->contents == NULL)
    // The directory is empty
    return (status = ERR_NOSUCHFILE);

  fileEntry2File((kernelFileEntry *) directory->contents, fileStructure);

  // Return success
  return (status = 0);
}


int kernelFileNext(const char *path, file *fileStructure)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fixedPath[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *directory = NULL;
  kernelFileEntry *listItem = NULL;

  // Do not look for files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Directory path for lookup is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the file structure we were passed is not NULL
  if (fileStructure == NULL)
    {
      kernelError(kernel_error, "File structure for lookup is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fixedPath);
  if (status < 0)
    {
      kernelError(kernel_error, "Error doing fixup of path");
      return (status);
    }

  // Make the starting directory correspond to the path we were given
  directory = kernelFileLookup(fixedPath);

  // Make sure it's good, and that it's really a subdirectory
  if ((directory == NULL) || (directory->type != dirT))
    {
      kernelError(kernel_error, "Invalid directory for lookup");
      return (status = ERR_NOSUCHFILE);
    }

  // Find the previously accessed file in the current directory.  Start 
  // with the first entry.
  if (directory->contents != NULL)
    listItem = (kernelFileEntry *) directory->contents;
  else 
    {
      kernelError(kernel_error, "No file entries in directory");
      return (status = ERR_NOSUCHFILE);
    }

  while (strcmp((char *) listItem->name, fileStructure->name)
	 && (listItem->nextEntry != NULL))
    listItem = (kernelFileEntry *) listItem->nextEntry;

  if (!strcmp((char *) listItem->name, fileStructure->name)
      && (listItem->nextEntry != NULL))
    {
      // Now we've found that last item.  Move one more down the list
      listItem = (kernelFileEntry *) listItem->nextEntry;
      
      fileEntry2File(listItem, fileStructure);
      fileStructure->handle = (void *) NULL;  // INVALID
    }
  
  else
    // There are no more files
    return (status = ERR_NOSUCHFILE);

  // Make sure the directory and files have their "last accessed" 
  // fields updated

  // Return success
  return (status = 0);
}


int kernelFileFind(const char *path, file *fileStructure)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *item = NULL;

  // Do not look for files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path name for lookup is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the file structure we were passed is not NULL
  if (fileStructure == NULL)
    {
      kernelError(kernel_error, "File structure for lookup is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error fixing up path");
      return (status);
    }

  // Call the routine that actually finds the item
  item = kernelFileLookup(fileName);
  if (item == NULL)
    {
      // There is no such item
      return (status = ERR_NOSUCHFILE);
    } 

  // Otherwise, we are OK, we got a good file.  We should now copy the
  // relevant information from the kernelFileEntry structure into the user
  // structure.

  fileEntry2File(item, fileStructure);
  fileStructure->handle = (void *) NULL;  // INVALID

  // Return success
  return (status = 0);
}


int kernelFileCreate(const char *path)
{
  // This is internal, and gets called by the open() routine when the
  // file in question needs to be created.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  char prefix[MAX_PATH_LENGTH];
  char name[MAX_NAME_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *directory = NULL;
  kernelFileEntry *createItem = NULL;

  // Do not create any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path name for file creation is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error fixing up path");
      return (status);
    }

  // Now make sure that the requested file does NOT exist
  createItem = kernelFileLookup(fileName);
  if (createItem != NULL)
    {
      kernelError(kernel_error, "File to create already exists");
      return (status = ERR_ALREADY);
    }

  // We have to find the directory where the user wants to create the 
  // file.  That's all but the last part of the "fileName" argument to 
  // you and I.  We have to call this function to separate the two parts.
  status = kernelFileSeparateLast(fileName, prefix, name);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing file path");
      return (status);
    }

  // Make sure "name" isn't empty.  This could happen if there WAS no
  // last item in the path to separate
  if (name[0] == NULL)
    {
      // Basically, we have been given an invalid name to open
      kernelError(kernel_error, "File to create has an invalid path");
      return (status = ERR_NOSUCHFILE);
    }

  // Now make sure that the requested directory exists
  directory = kernelFileLookup(prefix);
  if (directory == NULL)
    {
      // The directory does not exist
      kernelError(kernel_error, "Parent directory does not exist");
      return (status = ERR_NOSUCHDIR);
    }
      
  // OK, the directory exists.  Get the filesystem of the parent directory.
  theFilesystem = (kernelFilesystem *) directory->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "Unable to determine target filesystem");
      return (status = ERR_BADDATA);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // We can create the file in the directory.
  // Get a free file entry structure.
  createItem = kernelFileNewEntry(theFilesystem);
  if (createItem == NULL)
    {
      // There are currently no free file entry structures
      kernelError(kernel_error,
		  "No free file structures.  Unable to create.");
      return (status = ERR_NOFREE);
    }

  // Set up the appropriate data items in the new entry that aren't done
  // by default in the kernelFileNewEntry() function
  strncpy((char *) createItem->name, name, MAX_NAME_LENGTH);
  ((char *) createItem->name)[MAX_NAME_LENGTH - 1] = '\0';
  createItem->type = fileT;

  // Add the file to the directory
  status = kernelFileInsertEntry(createItem, directory);
  if (status < 0)
    {
      // We couldn't add the new entry to the directory.  We'd better 
      // put it back the free list
      kernelError(kernel_error,
		  "Unable to place new file in parent directory");
      kernelFileReleaseEntry(createItem->nextEntry);
      return (status);
    }

  // Check the filesystem driver function for creating files.  If it
  // exists, call it.
  if (theDriver->driverCreateFile != NULL)
    {
      status = theDriver->driverCreateFile(createItem);
      if (status < 0)
	return (status);
    }

  // Update the timestamps on the parent directory
  updateModifiedTime(directory);
  updateAccessedTime(directory);

  // Update the directory
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver->driverWriteDir(directory);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}

 
int kernelFileOpen(const char *fullPath, int openMode, file *fileStructure)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *directory = NULL;
  kernelFileEntry *openItem = NULL;
  int deleteSecure = 0;

  // Do not open any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Check params
  if ((fullPath == NULL) || (fileStructure == NULL))
    return (status = ERR_NULLPARAMETER);

  // Fix up the path name
  status = kernelFileFixupPath(fullPath, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path to open");
      return (status);
    }

  // First thing's first.  We might be able to short-circuit this whole
  // process if the file exists already.  Look for it.
  openItem = kernelFileLookup(fileName);

  // Does it exist?
  if (openItem == NULL)
    {
      // The file doesn't currently exist.  Were we told to create it?
      if (!(openMode & OPENMODE_CREATE))
	{
	  // We aren't supposed to create the file, so return an error
	  kernelError(kernel_error, "File to open does not exist");
	  return (status = ERR_NOSUCHFILE);
	}

      // We're going to create the file.
      status = kernelFileCreate(fileName);
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to create file");
	  return (status);
	}

      // Now find it.
      openItem = kernelFileLookup(fileName);
      if (openItem == NULL)
	{
	  // Something is really wrong
	  kernelError(kernel_error, "Unable to look up created file");
	  return (status = ERR_NOCREATE);
	}
    }

  // Make sure the item is really a file, and not a directory or anything else
  if (openItem->type != fileT)
    return (status = ERR_NOTAFILE);

  // Get the parent directory of the file
  directory = (kernelFileEntry *) openItem->parentDirectory;

  // Get the filesystem that the file belongs to
  theFilesystem = (kernelFilesystem *) openItem->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  if ((openMode & OPENMODE_WRITE) && theFilesystem->readOnly)
    // Not allowed in read-only file system
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // There are extra things we need to do if this will be a write operation

  if (openMode & OPENMODE_WRITE)
    {
      // Are we supposed to truncate the file first?
      if (openMode & OPENMODE_TRUNCATE)
	{
	  // Call the filesystem driver and ask it to delete the file.
	  // Then, we ask it to create the file again.

	  // Check the driver functions we want to use
	  if ((theDriver->driverDeleteFile == NULL) ||
	      (theDriver->driverCreateFile == NULL))
	    {
	      kernelError(kernel_error, "The requested filesystem operation "
			  "is not supported");
	      return (status = ERR_NOSUCHFUNCTION);
	    }

	  // Are we supposed to delete this file securely?
	  if (openItem->flags & FLAG_SECUREDELETE)
	    deleteSecure = 1;

	  status = theDriver->driverDeleteFile(openItem, deleteSecure);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Unable to truncate existing file");
	      return (status);
	    }
	  
	  status = theDriver->driverCreateFile(openItem);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Unable to create file");
	      return (status);
	    }
	}

      // Put a write lock on the file
      status = kernelLockGet(&(openItem->lock));
      if (status < 0)
	{
	  kernelError(kernel_error, "Unable to lock file for writing");
	  return (status);
	}

      // Update the modified dates/times on the file
      updateModifiedTime(openItem);
    }

  // Increment the open count on the file
  openItem->openCount += 1;

  // Update the access times/dates on the file
  updateAccessedTime(openItem);

  // Set up the file handle
  fileEntry2File(openItem, fileStructure);

  // Set the "open mode" flags on the opened file
  fileStructure->openMode = openMode;

  // Return success
  return (status = 0);
}


int kernelFileClose(file *fileStructure)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  kernelFileEntry *theFile = NULL;

  // Do not close any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the file pointer isn't NULL
  if (fileStructure == NULL)
    {
      kernelError(kernel_error, "File structure to close is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the file handle refers to a valid file
  theFile = (kernelFileEntry *) fileStructure->handle;
  if (theFile == NULL)
    {
      kernelError(kernel_error,
		  "File structure to close has a NULL file handle");
      return (status = ERR_NULLPARAMETER);
    }

  // Reduce the open count on the file in question
  if (theFile->openCount > 0)
    theFile->openCount -= 1;

  // If the file was locked by this PID, we should unlock it
  kernelLockRelease(&(theFile->lock));

  // Return success
  return (status = 0);
}


int kernelFileRead(file *fileStructure, unsigned blockNum, 
		   unsigned blocks, unsigned char *fileBuffer)
{
  // This function is responsible for all reading of files.  It takes a
  // file structure of an opened file, the number of pages to skip before
  // commencing the read, the number of pages to read, and a buffer to use.
  // Returns negative on error, otherwise it returns the same status as
  // the driver function.

  int status = 0;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  
  // Do not read any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Check parameters
  if ((fileStructure == NULL) || (fileBuffer == NULL))
    {
      kernelError(kernel_error, "NULL file or buffer structure");
      return (status = ERR_NULLPARAMETER);
    }
  if (fileStructure->handle == NULL)
    {
      kernelError(kernel_error, "NULL file handle");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the number of blocks to read is non-zero
  if (!blocks)
    // This isn't an error exactly, there just isn't anything to do.
    return (status = 0);

  // Has the file been opened properly for reading?  Maybe we only need
  // to do this check if we're writing.  Maybe it's OK to be pedantic
  // however. :)  No, that sucks.
  // if (!(fileStructure->openMode & OPENMODE_READ))
  //   {
  //     kernelError(kernel_error, "File has not been opened for reading");
  //     return (status = ERR_INVALID);
  //   }

  // Get the filesystem object based on the filesystem number in the
  // file structure
  theFilesystem = ((kernelFileEntry *) fileStructure->handle)->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // OK, we just have to check on the filesystem driver function we want
  // to call
  if (theDriver->driverReadFile == NULL)
    {
      kernelError(kernel_error, "The requested filesystem operation is not "
		  "supported");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Now we can call our target function
  status = theDriver->driverReadFile(fileStructure->handle, blockNum,
				     blocks, fileBuffer);

  // Make sure the file structure is up to date after the call
  fileEntry2File(fileStructure->handle, fileStructure);
  
  // Return the same error status that the driver function returned
  return (status);
}


int kernelFileWrite(file *fileStructure, unsigned blockNum,
		    unsigned blocks, unsigned char *fileBuffer)
{
  // This function is responsible for all writing of files.  It takes a
  // file structure of an opened file, the number of blocks to skip before
  // commencing the write, the number of blocks to write, and a buffer to use.
  // Returns negative on error, otherwise it returns the same status as
  // the driver function.

  int status = 0;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  
  // Do not read any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Check parameters
  if ((fileStructure == NULL) || (fileBuffer == NULL))
    {
      kernelError(kernel_error, "NULL file or buffer structure");
      return (status = ERR_NULLPARAMETER);
    }
  if (fileStructure->handle == NULL)
    {
      kernelError(kernel_error, "NULL file handle");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the number of blocks to write is non-zero
  if (!blocks)
    // Eek.  Nothing to do.  Why were we called?
    return (status = ERR_NODATA);

  // Has the file been opened properly for writing?
  if (!(fileStructure->openMode & OPENMODE_WRITE))
    {
      kernelError(kernel_error, "File has not been opened for writing");
      return (status = ERR_INVALID);
    }

  // Get the filesystem object based on the filesystem number in the
  // file structure
  theFilesystem = ((kernelFileEntry *) fileStructure->handle)->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // OK, we just have to check on the filesystem driver function we want
  // to call
  if (theDriver->driverWriteFile == NULL)
    {
      kernelError(kernel_error, "The requested filesystem operation is not "
		  "supported");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Now we can call our target function
  status = theDriver->driverWriteFile(fileStructure->handle, blockNum,
				      blocks, fileBuffer);
  if (status < 0)
    return (status);
  
  // Update the directory
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver
	->driverWriteDir(((kernelFileEntry *) fileStructure->handle)
			 ->parentDirectory);
      if (status < 0)
	return (status);
    }

  // Make sure the file structure is up to date after the call
  fileEntry2File(fileStructure->handle, fileStructure);
  
  // Return the same error status that the driver function returned
  return (status = 0);
}


int kernelFileDelete(const char *path)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *theFile = NULL;
  kernelFileEntry *parentDir = NULL;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  int secureDelete = 0;

  // Do not delete any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path to delete is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path to delete");
      return (status);
    }

  // Now make sure that the requested file exists
  theFile = kernelFileLookup(fileName);
  if (theFile == NULL)
    {
      // The directory does not exist
      kernelError(kernel_error, "File to delete does not exist");
      return (status = ERR_NOSUCHFILE);
    }

  // OK, the file exists.

  // Record the parent directory before we nix the file
  parentDir = (kernelFileEntry *) theFile->parentDirectory;
  
  // Make sure the item is really a file, and not a directory.  We 
  // have to do different things for removing directory
  if (theFile->type != fileT)
    {
      kernelError(kernel_error, "Item to delete is not a file");
      return (status = ERR_NOTAFILE);
    }

  // Are we doing a 'secure delete' on this file?
  if (theFile->flags & FLAG_SECUREDELETE)
    secureDelete = 1;

  // Figure out which filesystem we're using
  theFilesystem = (kernelFilesystem *) theFile->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // If the filesystem driver has a 'delete' function, call it.
  if (theDriver->driverDeleteFile != NULL)
    status = theDriver->driverDeleteFile(theFile, secureDelete);
  if (status < 0)
    // Don't quit here.  This is the filesystem driver's problem
    kernelError(kernel_warn, "Filesystem driver did not release the file "
		"data");

  // Remove the entry for this file from its parent directory
  status = kernelFileRemoveEntry(theFile);
  if (status < 0)
    {
      kernelError(kernel_error, 
		  "Couldn't remove file from its parent directory");
      return (status);
    }

  // Deallocate the data structure
  kernelFileReleaseEntry(theFile);

  // Update the times on the directory
  updateModifiedTime(parentDir);
  updateAccessedTime(parentDir);

  // Update the directory
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver->driverWriteDir(parentDir);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFileDeleteSecure(const char *path)
{
  // This is a wrapper function for the 'delete' function, above.  This
  // will set the file's FLAG_SECUREDELETE and pass on the request.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *theFile = NULL;

  // Do not delete any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path to delete is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path to delete");
      return (status);
    }

  // Now make sure that the requested file exists
  theFile = kernelFileLookup(fileName);
  if (theFile == NULL)
    {
      // The directory does not exist
      kernelError(kernel_error, "File to delete does not exist");
      return (status = ERR_NOSUCHFILE);
    }

  // OK, the file exists.  Set FLAG_SECUREDELETE on the file.
  theFile->flags |= FLAG_SECUREDELETE;

  // Call the regular 'delete' function
  return (kernelFileDelete(path));
}


int kernelFileMakeDir(const char *path)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fixedPath[MAX_PATH_NAME_LENGTH];
  char prefix[MAX_PATH_LENGTH];
  char name[MAX_NAME_LENGTH];
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  kernelFileEntry *parent = NULL;
  kernelFileEntry *newDir = NULL;

  // Do not make any directories until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path of directory to create is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fixedPath);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of directory to create");
      return (status);
    }

  // We must separate the name of the new file from the requested path
  status = kernelFileSeparateLast(fixedPath, prefix, name);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of directory to create");
      return (status);
    }

  // Make sure "name" isn't empty.  This could happen if there WAS no
  // last item in the path to separate (like 'mkdir /', which would of
  // course be invalid)
  if (name[0] == NULL)
    {
      // Basically, we have been given an invalid directory name to create
      kernelError(kernel_error, "Path of directory to create is invalid");
      return (status = ERR_NOSUCHFILE);
    }

  // Now make sure that the requested parent directory exists
  parent = kernelFileLookup(prefix);
  if (parent == NULL)
    {
      // The directory does not exist
      kernelError(kernel_error, "Parent directory does not exist");
      return (status = ERR_NOSUCHDIR);
    }
  
  // Make sure it's a directory
  if (parent->type != dirT)
    {
      kernelError(kernel_error, "Parent directory is not a directory");
      return (status = ERR_NOSUCHDIR);
    }

  // Figure out which filesystem we're using
  theFilesystem = (kernelFilesystem *) parent->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // Allocate a new file entry 
  newDir = kernelFileNewEntry(theFilesystem);
  if (newDir == NULL)
    {
      // Not enough free file structures
      kernelError(kernel_error, "Not enough free file structures");
      return (status = ERR_NOFREE);
    }

  // Now, set some fields for our new item

  strncpy((char *) newDir->name, name, MAX_NAME_LENGTH);
  ((char *) newDir->name)[MAX_NAME_LENGTH - 1] = '\0';

  newDir->type = dirT;

  // Set the creation, modification times, etc.
  updateCreationTime(newDir);
  updateModifiedTime(newDir);
  updateAccessedTime(newDir);

  // Now add the new directory to its parent
  status = kernelFileInsertEntry(newDir, parent);
  if (status < 0)
    {
      // We couldn't add the directory for whatever reason.  In that case
      // we must attempt to deallocate the cluster we allocated, and return 
      // the three files we created to the free list
      kernelError(kernel_error, "Error adding new directory to parent");
      kernelFileReleaseEntry(newDir);
      return (status);
    }

  // Create the '.' and '..' entries inside the directory
  status = kernelFileMakeDotDirs(theFilesystem, parent, newDir);
  if (status < 0)
    kernelError(kernel_warn, "Unable to create '.' and '..' directory "
		"entries");

  // If the filesystem driver has a 'make dir' routine, call it.
  if (theDriver->driverMakeDir != NULL)
    {
      status = theDriver->driverMakeDir(newDir);
      if (status < 0)
	return (status);
    }

  // Write both directories
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver->driverWriteDir(newDir);
      if (status < 0)
	return (status);

      status = theDriver->driverWriteDir(parent);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFileRemoveDir(const char *path)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *theDir = NULL;
  kernelFileEntry *parentDir = NULL;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;

  // Do not delete any directories until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path of directory to remove is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of directory to remove");
      return (status);
    }

  // Ok, do NOT remove the root directory
  if (strcmp(fileName, "/") == 0)
    {
      kernelError(kernel_error,
		  "Cannot remove the root directory under any circumstances");
      return (status = ERR_NODELETE);
    }

  // Now make sure that the requested directory exists
  theDir = kernelFileLookup(fileName);
  if (theDir == NULL)
    {
      // The directory does not exist
      kernelError(kernel_error, "Directory to delete does not exist");
      return (status = ERR_NOSUCHDIR);
    }

  // Make sure the item to delete is really a directory
  if (theDir->type != dirT)
    {
      kernelError(kernel_error, "Item to delete is not a directory");
      return (status = ERR_NOTADIR);
    }
  
  // Make sure the directory to delete is empty
  if (!dirIsEmpty(theDir))
    {
      kernelError(kernel_error, "Directory to delete is not empty");
      return (status = ERR_NOTEMPTY);
    }

  // Record the parent directory
  parentDir = theDir->parentDirectory;

  // Figure out which filesystem we're using
  theFilesystem = (kernelFilesystem *) theDir->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // If the filesystem driver has a 'remove dir' function, call it.
  if (theDriver->driverRemoveDir != NULL)
    {
      status = theDriver->driverRemoveDir(theDir);
      if (status < 0)
	return (status);
    }

  // Now remove the '.' and '..' entries from the directory
  while (theDir->contents != NULL)
    {
      status = kernelFileRemoveEntry((kernelFileEntry *) theDir->contents);
      if (status < 0)
	return (status);

      kernelFileReleaseEntry((kernelFileEntry *) theDir->contents);
    }

  // Now remove the directory from its parent
  status = kernelFileRemoveEntry(theDir);
  if (status < 0)
    return (status);

  kernelFileReleaseEntry(theDir);

  // Update the times on the parent directory
  updateModifiedTime(parentDir);
  updateAccessedTime(parentDir);

  // Write the directory
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver->driverWriteDir(parentDir);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFileCopy(const char *srcPath, const char *destPath)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  file srcFileStruct;
  file destFileStruct;
  char destFileName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *destFile = NULL;
  void *fileBuffer = NULL;
  unsigned destBlocks = 0;

  // Do not copy any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure neither the source or destination file names are NULL
  if ((srcPath == NULL) || (destPath == NULL))
    {
      kernelError(kernel_error, "Path name pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Attempt to open the source file for reading (the open function will
  // check the source name argument for us)
  status = kernelFileOpen(srcPath, OPENMODE_READ, &srcFileStruct);
  if (status < 0)
    {
      kernelError(kernel_error, "Couldn't open source file");
      return (status);
    }

  status = kernelFileFixupPath(destPath, destFileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of destination file");
      kernelFileClose(&srcFileStruct);
      return (status);
    }

  // Determine whether the destination file exists, and if so whether it
  // is a directory.
  destFile = kernelFileLookup(destFileName);
  
  if (destFile != NULL)
    {
      // It already exists.  Is it a directory?
      if (destFile->type == dirT)
	{
	  // It's a directory, so what we really want to do is make a
	  // destination file in that directory that shares the same filename
	  // as the source.  Construct the new name.
	  strncat(destFileName, "/", 1);
	  strcat(destFileName, srcFileStruct.name);

	  if (kernelFileLookup(destFileName))
	    {
	      // Remove the existing file
	      status = kernelFileDelete(destFileName);
	      if (status < 0)
		{
		  kernelError(kernel_error, "Cannot delete existing "
			      "destination file");
		  return (status);
		}
	    }
	}
    }

  // Is there enough space in the destination filesystem for the copied
  // file?
  if (srcFileStruct.size > kernelFilesystemGetFree(srcFileStruct.filesystem))
    {
      kernelError(kernel_error, "Not enough space in destination filesystem");
      kernelFileClose(&srcFileStruct);
      return (status = ERR_NOFREE);
    }

  // Attempt to open the destination file for writing.
  status = kernelFileOpen(destFileName, (OPENMODE_WRITE | OPENMODE_CREATE | 
					 OPENMODE_TRUNCATE), &destFileStruct);
  if (status < 0)
    {
      kernelError(kernel_error, "Couldn't open destination file");
      kernelFileClose(&srcFileStruct);
      return (status);
    }

  destFile = (kernelFileEntry *) destFileStruct.handle;

  // Make sure the destination file's block size isn't zero (this would
  // lead to a divide-by-zero error at writing time)
  if (destFileStruct.blockSize == 0)
    {
      kernelError(kernel_error, "Destination file has zero blocksize");
      kernelFileClose(&srcFileStruct);
      return (status = ERR_DIVIDEBYZERO);
    }

  // Any data to copy?
  if (srcFileStruct.size > 0)
    {
      // Copy the data.  Allocate a buffer large enough to hold the entire
      // source file
      
      fileBuffer =
	kernelMalloc(srcFileStruct.blocks * srcFileStruct.blockSize);
      if (fileBuffer == NULL)
	{
	  kernelError(kernel_error, "Not enough memory to copy file");
	  kernelFileClose(&srcFileStruct);
	  kernelFileClose(&destFileStruct);
	  return (status = ERR_MEMORY);
	}

      // Read the source file
      status = kernelFileRead(&srcFileStruct, 0, srcFileStruct.blocks,
			      fileBuffer);
      if (status < 0)
	{
	  kernelError(kernel_error, "Error reading source file");
	  kernelFileClose(&srcFileStruct);
	  kernelFileClose(&destFileStruct);
	  kernelFree(fileBuffer);
	  return (status);
	}
      
      // Calculate the number of blocks that will be used by the destination
      // file.  It might be different than the source file because of
      // different block sizes.
      destBlocks = (srcFileStruct.size / destFileStruct.blockSize);
      if (srcFileStruct.size % destFileStruct.blockSize)
	destBlocks += 1;

      // Write the destination file
      status = kernelFileWrite(&destFileStruct, 0, destBlocks, fileBuffer);

      // Free the copy buffer
      kernelFree(fileBuffer);

      if (status < 0)
	{
	  kernelError(kernel_error, "Error %d writing destination file",
		      status);
	  kernelFileClose(&srcFileStruct);
	  kernelFileClose(&destFileStruct);
	  return (status);
	}
    }

  // Close the files
  kernelFileClose(&srcFileStruct);
  kernelFileClose(&destFileStruct);
  
  // Set the size of the destination file so that it matches that of
  // the source file (as opposed to a multiple of the block size and
  // the number of blocks it consumes)
  status = kernelFileSetSize(&destFileStruct, srcFileStruct.size);
  if (status < 0)
    kernelError(kernel_warn, "Unable to set correct file size");

  // Return success
  return (status = 0);
}


int kernelFileCopyRecursive(const char *srcPath, const char *destPath)
{
  // This is a function to copy directories recursively.  The source name
  // can be a regular file as well; it will just copy the single file. 

  int status = 0;
  kernelFileEntry *src = NULL;
  kernelFileEntry *dest = NULL;
  char srcName[MAX_PATH_NAME_LENGTH];
  char destName[MAX_PATH_NAME_LENGTH];
  char tmpSrcName[MAX_PATH_NAME_LENGTH];
  char tmpDestName[MAX_PATH_NAME_LENGTH];

  // Do not copy any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure neither the source or destination file names are NULL
  if ((srcPath == NULL) || (destPath == NULL))
    {
      kernelError(kernel_error, "Path name pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = kernelFileFixupPath(srcPath, srcName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of source file");
      return (status);
    }

  status = kernelFileFixupPath(destPath, destName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of destination file");
      return (status);
    }

  // Determine whether the source item exists, and if so whether it
  // is a directory.
  src = kernelFileLookup(srcPath);
  if (src == NULL)
    {
      kernelError(kernel_error, "File to copy does not exist");
      return (status = ERR_NOSUCHENTRY);
    }

  // It exists.  Is it a directory?
  if (src->type == dirT)
    {
      // It's a directory, so we create the destination directory if it doesn't
      // already exist, then we loop through the entries in the source
      // directory.  If an entry is a file, copy it.  If it is a directory,
      // recurse.

      dest = kernelFileLookup(destName);
      if (dest == NULL)
	{
	  // Create the directory
	  status = kernelFileMakeDir(destName);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Unable to create destination "
			  "directory");
	      return (status);
	    }

	  dest = kernelFileLookup(destName);
	  if (dest == NULL)
	    {
	      // Something wrong here.
	      kernelError(kernel_error, "Unable to create destination "
			  "directory");
	      return (status = ERR_NOSUCHENTRY);
	    }
	}
      
      // Get the first file in the source directory
      src = src->contents;

      // Skip any '.' and '..' entries
      while (!strcmp((char *) src->name, ".") ||
	     !strcmp((char *) src->name, ".."))
	src = src->nextEntry;

      while (src != NULL)
	{
	  // Add the file's name to the directory's name
	  strcpy(tmpSrcName, srcName);
	  strcat(tmpSrcName, "/");
	  strcat(tmpSrcName, (const char *) src->name);
	  // Add the file's name to the destination file name
	  strcpy(tmpDestName, destName);
	  strcat(tmpDestName, "/");
	  strcat(tmpDestName, (const char *) src->name);

	  status = kernelFileCopyRecursive(tmpSrcName, tmpDestName);
	  if (status < 0)
	    return (status);
	  
	  src = src->nextEntry;
	}
    }
  else
    {
      // Just copy the file using the existing copy function
      return (kernelFileCopy(srcPath, destPath));
    }
  
  // Return success
  return (status = 0);
}


int kernelFileMove(const char *srcPath, const char *destPath)
{
  // This function is used to move or rename one file or directory to 
  // another location or name.  Of course, this involves moving no actual 
  // data, but rather moving the references to the data from one location 
  // to another.  If the target location is the name of a directory, the
  // specified item is moved to that directory and will have the same
  // name as before.  If the target location is a file name, the item
  // will be moved and renamed.  Returns 0 on success, negtive otherwise.

  int status = 0;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;
  char srcFileName[MAX_PATH_NAME_LENGTH];
  char destFileName[MAX_PATH_NAME_LENGTH];
  char destDirPath[MAX_PATH_LENGTH];
  char origName[MAX_NAME_LENGTH];
  char newName[MAX_NAME_LENGTH];
  kernelFileEntry *sourceFile = NULL;
  kernelFileEntry *sourceDir = NULL;
  kernelFileEntry *destFile = NULL;
  kernelFileEntry *destDir = NULL;

  // Do not move anything until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure neither the source or destination file names are NULL
  if ((srcPath == NULL) || (destPath == NULL))
    {
      kernelError(kernel_error, "Path name pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the source and destination path names
  status = kernelFileFixupPath(srcPath, srcFileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path name of source file");
      return (status);
    }

  status = kernelFileFixupPath(destPath, destFileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path name of destination "
		  "file");
      return (status);
    }

  // Now make sure that the requested source item exists.  Whether it is a
  // file or directory is unimportant.
  sourceFile = kernelFileLookup(srcFileName);
  if (sourceFile == NULL)
    {
      // The directory does not exist
      kernelError(kernel_error, "Source item does not exist");
      return (status = ERR_NOSUCHFILE);
    }

  // Save the source directory
  sourceDir = sourceFile->parentDirectory;

  // Figure out which filesystem we're using for the source file based 
  // on the path name we were passed
  theFilesystem = (kernelFilesystem *) sourceFile->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // Save the source file's original name, just in case we encounter
  // problems later
  strcpy(origName, (char *) sourceFile->name);

  // By default, the new name is the same as the old name
  strcpy(newName, (char *) sourceFile->name);

  // Check whether the destination file exists.  If it exists and it is
  // a file, we need to delete it first.  
  destFile = kernelFileLookup(destFileName);

  if (destFile != NULL)
    {
      // It already exists.  Is it a directory?
      if (destFile->type == dirT)
	{
	  // It's a directory, so what we really want to do is make a
	  // destination file in that directory that shares the same filename
	  // as the source.  Construct the new name.
	  strncat(destFileName, "/", 1);
	  strcat(destFileName, newName);
	  
	  if (kernelFileLookup(destFileName))
	    {
	      // Remove the existing file
	      status = kernelFileDelete(destFileName);
	      if (status < 0)
		{
		  kernelError(kernel_error, "Cannot delete existing "
			      "destination file");
		  return (status);
		}
	    }

	  // The specified destination is the destination directory.  The
	  // moved file keeps the same name
	  destDir = destFile;
	}

      else
	{
	  // Save the pointer to the parent directory
	  destDir = destFile->parentDirectory;

	  // Set the dest file's name.
	  strcpy(newName, (char *) destFile->name);

	  // Delete the existing file
	  status = kernelFileDelete(destFileName);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Cannot delete existing destination "
			  "file");
	      return (status);
	    }
	}
    }
  else
    {
      // No such file exists.  We need to get the destination directory
      // from the destination path we were supplied.
      status = kernelFileSeparateLast(destFileName, destDirPath, newName);
      if (status < 0)
	{
	  kernelError(kernel_error, "Destination directory does not exist");
	  return (status);
	}

      destDir = kernelFileLookup(destDirPath);
      if (destDir == NULL)
	{
	  kernelError(kernel_error, "Destination directory does not exist");
	  return (status = ERR_NOSUCHDIR);
	}
    }

  // Now check the filesystem of the destination directory.  Here's the
  // trick: moves can only occur within a single filesystem.
  if ((kernelFilesystem *) destDir->filesystem != theFilesystem)
    {
      kernelError(kernel_error, "Can only move items within a single "
		  "filesystem");
      return (status = ERR_INVALID);
    }

  // If the source item is a directory, make sure that the destination
  // directory is not a descendent of the source.  Moving a directory tree
  // into one of its own subtrees is paradoxical.  Also make sure that the
  // directory being moved is not the same as the destination directory
  if (sourceFile->type == dirT)
    if (isDescendent(destDir, sourceFile))
      {
	// The destination is a descendant of the source
	kernelError(kernel_error, "Cannot move directory into one of its "
		    "own subdirectories");
	return (status = ERR_PARADOX);
      }
      
  // Remove the source file from its current directory
  status = kernelFileRemoveEntry(sourceFile);
  if (status < 0)
    {
      // We were unable to remove the entry from its current directory.
      kernelError(kernel_error, "Unable to remove source file from current "
		  "directory");
      return (status);
    }

  // Change the souce item's filename if necessary.  It might be the same
  // or it might be different.
  strcpy((char *) sourceFile->name, newName);

  // Place the file in its new directory
  status = kernelFileInsertEntry(sourceFile, destDir);
  if (status < 0)
    {
      kernelError(kernel_error, "Unable to place source file in new "
		  "directory");

      // We were unable to place it in the new directory.  Better at
      // least try to put it back where it belongs in its old directory, 
      // with its old name.  Whether this succeeds or fails, we need to try
      strcpy((char *) sourceFile->name, origName);
      kernelFileInsertEntry(sourceFile, sourceDir);
      return (status);
    }

  // Update some of the data items in the file entries
  updateAccessedTime(sourceFile);
  updateModifiedTime(sourceDir);
  updateAccessedTime(sourceDir);
  updateModifiedTime(destDir);
  updateAccessedTime(destDir);

  // If the filesystem driver has a driverFileMoved function, call it
  if (theDriver->driverFileMoved != NULL)
    {
      status = theDriver->driverFileMoved(sourceFile);
      if (status < 0)
	return (status);
    }

  // Write both directories
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver->driverWriteDir(destDir);
      if (status < 0)
	return (status);
      status = theDriver->driverWriteDir(sourceDir);
      if (status < 0)
	return (status);
    }

  // Return success
  return (status = 0);
}


int kernelFileSetSize(file *theFile, unsigned newSize)
{
  // This file allows the caller to specify the real size of a file, since
  // the other routines here must assume that the file consumes all of the
  // space in all of its blocks (they have no way to know otherwise).

  int status = 0;
  kernelFileEntry *entry = NULL;
  kernelFilesystem *theFilesystem = NULL;

  // Check parameters
  if (theFile == NULL)
    return (status = ERR_NULLPARAMETER);

  // The kernelFileEntry structure
  entry = (kernelFileEntry *) theFile->handle;
  if (entry == NULL)
    return (status = ERR_INVALID);

  theFilesystem = (kernelFilesystem *) entry->filesystem;

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  // Make sure the new size is acceptable.  Basically, the number we are
  // being given must fall within the possible values consistent with the
  // number of blocks that are allocated to this file.
  if ((newSize != 0) || (entry->blocks != 0))
    {
      if (newSize < ((entry->blocks - 1) * theFilesystem->blockSize))
	{
	  kernelError(kernel_error, "New size for file is too small");
	  return (status = ERR_INVALID);
	}
      else if (newSize > (entry->blocks * theFilesystem->blockSize))
	{
	  kernelError(kernel_error, "New size for file is too large");
	  return (status = ERR_INVALID);
	}
    }

  // The value is OK.  Try to set it in both the file structure and the
  // kernelFileEntry structure.
  
  entry->size = newSize;
  entry->blocks = (entry->size / theFilesystem->blockSize);
  if (entry->size % theFilesystem->blockSize)
    entry->blocks += 1;

  if (((kernelFilesystemDriver *) theFilesystem->driver)
      ->driverWriteDir != NULL)
    {
      status = ((kernelFilesystemDriver *) theFilesystem->driver)
	->driverWriteDir((kernelFileEntry *) entry->parentDirectory);
      if (status < 0)
	return (status);
    }

  // Update the file structure
  fileEntry2File(entry, theFile);

  // Return success
  return (status = 0);
}


int kernelFileTimestamp(const char *path)
{
  // This is merely a wrapper function for the equivalent function
  // in the requested filesystem's own driver.  It takes nearly-identical
  // arguments and returns the same status as the driver function.

  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  kernelFileEntry *theFile = NULL;
  kernelFilesystem *theFilesystem = NULL;
  kernelFilesystemDriver *theDriver = NULL;

  // Do not timestamp any files until we have been initialized
  if (!initialized)
    {
      kernelError(kernel_error, "The file manager has not been initialized");
      return (status = ERR_NOTINITIALIZED);
    }  

  // Make sure the path name we were passed is not NULL
  if (path == NULL)
    {
      kernelError(kernel_error, "Path name of file to timestamp is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Fix up the path name
  status = kernelFileFixupPath(path, fileName);
  if (status < 0)
    {
      kernelError(kernel_error, "Error parsing path of file to timestamp");
      return (status);
    }

  // Now make sure that the requested file exists
  theFile = kernelFileLookup(fileName);
  if (theFile == NULL)
    {
      kernelError(kernel_error, "File to timestamp does not exist");
      return (status = ERR_NOSUCHFILE);
    }

  // Now we change the internal data fields of the file's data structure
  // to match the current date and time (Don't touch the creation date/time)

  // Set the file's "last modified" and "last accessed" times
  updateModifiedTime(theFile);
  updateAccessedTime(theFile);

  // Now allow the underlying filesystem driver to do anything that's
  // specific to that filesystem type.
  theFilesystem = (kernelFilesystem *) theFile->filesystem;
  if (theFilesystem == NULL)
    {
      kernelError(kernel_error, "NULL filesystem pointer");
      return (status = ERR_BADADDRESS);
    }

  // Not allowed in read-only file system
  if (theFilesystem->readOnly)
    return (status = ERR_NOWRITE);

  theDriver = (kernelFilesystemDriver *) theFilesystem->driver;

  // Call the filesystem driver function, if it exists
  if (theDriver->driverTimestamp != NULL)
    {
      status = theDriver->driverTimestamp(theFile);
      if (status < 0)
	return (status);
    }

  // Write the directory
  if (theDriver->driverWriteDir != NULL)
    {
      status = theDriver
	->driverWriteDir((kernelFileEntry *) theFile->parentDirectory);
      if (status < 0)
	return (status);
    }

  return (status = 0);
}


int kernelFileUnbufferRecursive(kernelFileEntry *dir)
{
  // This function can be used to unbuffer a directory tree recursively.
  // Mostly, this will be useful only when unmounting filesystems.
  // Returns 0 on success, negative otherwise.

  int status = 0;
  kernelFileEntry *listItemPointer = NULL;

  // Is dir a directory?
  if (dir->type != dirT)
    return (status = ERR_NOTADIR);

  // Is the current directory a leaf directory?
  if (!isLeafDir(dir))
    {
      // It's not a leaf directory.  We will need to walk through each of
      // the entries and do a recursion for any subdirectories that have
      // buffered content.
      listItemPointer = dir->contents;
      while (listItemPointer != NULL)
	{
	  if ((listItemPointer->type == dirT) &&
	      (listItemPointer->contents != NULL))
	    {
	      status = kernelFileUnbufferRecursive(listItemPointer);
	      if (status < 0)
		return (status);
	    }
	  else
	    listItemPointer = (kernelFileEntry *) listItemPointer->nextEntry;
	}
    }

  // Now this directory should be a leaf directory.  Do any of its files
  // currently have a valid lock?
  listItemPointer = (kernelFileEntry *) dir->contents;
  while (listItemPointer != NULL)
    {
      if (kernelLockVerify(&(listItemPointer->lock)))
	break;

      else
	listItemPointer = (kernelFileEntry *) listItemPointer->nextEntry;
    }

  if (listItemPointer != NULL)
    // There are open files here
    return (status = ERR_BUSY);

  // This directory is safe to unbuffer
  status = unbufferDirectory(dir);

  // Return the status from that call
  return (status);
}
