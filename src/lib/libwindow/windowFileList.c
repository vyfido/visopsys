// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  windowFileList.c
//

// This contains functions for user programs to operate GUI components.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/window.h>

#define FILEBROWSE_CONFIG        "/system/config/filebrowse.conf"
#define DEFAULT_FOLDERICON_VAR   "icon.folder"
#define DEFAULT_FOLDERICON_FILE  "/system/icons/foldicon.bmp"
#define DEFAULT_FILEICON_VAR     "icon.file"
#define DEFAULT_FILEICON_FILE    "/system/icons/pageicon.bmp"
#define DEFAULT_IMAGEICON_VAR    "icon.image"
#define DEFAULT_IMAGEICON_FILE   "/system/icons/imageicon.bmp"
#define DEFAULT_EXECICON_VAR     "icon.executable"
#define DEFAULT_EXECICON_FILE    "/system/icons/execicon.bmp"
#define DEFAULT_OBJICON_VAR      "icon.object"
#define DEFAULT_OBJICON_FILE     "/system/icons/objicon.bmp"
#define DEFAULT_CONFIGICON_VAR   "icon.config"
#define DEFAULT_CONFIGICON_FILE  "/system/icons/conficon.bmp"
#define DEFAULT_BOOTICON_VAR     "icon.boot"
#define DEFAULT_BOOTICON_FILE    "/system/icons/booticon.bmp"
#define DEFAULT_TEXTICON_VAR     "icon.text"
#define DEFAULT_TEXTICON_FILE    "/system/icons/texticon.bmp"
#define DEFAULT_BINICON_VAR      "icon.binary"
#define DEFAULT_BINICON_FILE     "/system/icons/binicon.bmp"

typedef struct {
  int classFlags;
  const char *imageVariable;
  const char *imageFile;
  image *image;

} icon;

typedef struct {
  file file;
  char fullName[MAX_PATH_NAME_LENGTH];
  listItemParameters iconParams;
  loaderFileClass class;
  icon *icon;

} fileEntry;

static variableList config;
static int initialized = 0;

// Our list of icon images
static image folderImage;
static image fileImage;
static image configImage;
static image textImage;
static image imageImage;
static image bootImage;
static image execImage;
static image objImage;
static image binImage;

#define FOLDER_ICON \
 { 0, DEFAULT_FOLDERICON_VAR, DEFAULT_FOLDERICON_FILE, &folderImage }
#define FILE_ICON \
 { 0xFFFFFFFF, DEFAULT_FILEICON_VAR, DEFAULT_FILEICON_FILE, &fileImage }

static icon folderIcon = FOLDER_ICON;
static icon fileIcon = FILE_ICON;

static icon iconList[] = {
  // These get traversed in order; the first matching file class flags get
  // the icon.  So, for example, if you want to make an icon for a type
  // of binary file, put it *before* the icon for plain binaries.
  { LOADERFILECLASS_CONFIG, DEFAULT_CONFIGICON_VAR, DEFAULT_CONFIGICON_FILE,
    &configImage },
  { LOADERFILECLASS_TEXT, DEFAULT_TEXTICON_VAR, DEFAULT_TEXTICON_FILE,
    &textImage },
  { LOADERFILECLASS_IMAGE, DEFAULT_IMAGEICON_VAR, DEFAULT_IMAGEICON_FILE,
    &imageImage },
  { LOADERFILECLASS_BOOT, DEFAULT_BOOTICON_VAR, DEFAULT_BOOTICON_FILE,
    &bootImage },
  { LOADERFILECLASS_EXEC, DEFAULT_EXECICON_VAR, DEFAULT_EXECICON_FILE,
    &execImage },
  { (LOADERFILECLASS_OBJ | LOADERFILECLASS_LIB), DEFAULT_OBJICON_VAR,
    DEFAULT_OBJICON_FILE, &objImage },
  { LOADERFILECLASS_BIN, DEFAULT_BINICON_VAR, DEFAULT_BINICON_FILE,
    &binImage },
  // This one goes last, because the flags match every file class.
  FILE_ICON
};
    

static void error(const char *, ...) __attribute__((format(printf, 1, 2)));
static void error(const char *format, ...)
{
  // Generic error message code
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  windowNewErrorDialog(NULL, "Error", output);
}


static int loadIcon(const char *variableName, const char *defaultIcon,
		    image *theImage)
{
  // Try to load the requested icon, first based on the configuration file
  // variable name, then by the default filename.

  int status = 0;
  char variableValue[MAX_PATH_NAME_LENGTH];
  file tmpFile;

  // First try the variable
  status = variableListGet(&config, variableName, variableValue,
			   MAX_PATH_NAME_LENGTH);
  if (status >= 0)
    defaultIcon = variableValue;

  // Try to load the image
  status = fileFind(defaultIcon, &tmpFile);
  if (status < 0)
    return (status);

  return (imageLoad(defaultIcon, 0, 0, theImage));
}


static void getFileIcon(fileEntry *entry)
{
  int count;

  entry->icon = &fileIcon;

  for (count = 0; count < (int) (sizeof(iconList) / sizeof(icon)); count ++)
    {
      if (entry->class.flags & iconList[count].classFlags)
	{
	  entry->icon = &iconList[count];
	  break;
	}
    }

  while (entry->icon->image->data == NULL)
    {
      if (loadIcon(entry->icon->imageVariable, entry->icon->imageFile,
		   entry->icon->image) < 0)
	{
	  if (entry->icon == &fileIcon)
	    return;

	  entry->icon = &fileIcon;
	}
      else
	break;
    }

  memcpy(&(entry->iconParams.iconImage), entry->icon->image, sizeof(image));
}


static int classifyEntry(fileEntry *entry)
{
  // Given a file entry with it's 'file' field filled, classify the file,
  // set up the icon image, etc.

  int status = 0;

  strncpy(entry->iconParams.text, entry->file.name, WINDOW_MAX_LABEL_LENGTH);

  switch (entry->file.type)
    {
    case dirT:
      if (!strcmp(entry->file.name, ".."))
	strcpy(entry->iconParams.text, "(up)");
      entry->icon = &folderIcon;
      if (entry->icon->image->data == NULL)
	{
	  status = loadIcon(entry->icon->imageVariable, entry->icon->imageFile,
			    entry->icon->image);
	  if (status < 0)
	    return (status);
	}
      memcpy(&(entry->iconParams.iconImage), entry->icon->image,
	     sizeof(image));
      break;

    case fileT:
      // Get the file class information
      loaderClassifyFile(entry->fullName, &(entry->class));

      // Get the the icon for the file
      getFileIcon(entry);

      break;

    case linkT:
      if (!strcmp(entry->file.name, ".."))
	{
	  strcpy(entry->iconParams.text, "(up)");
	  entry->icon = &folderIcon;
	  if (entry->icon->image->data == NULL)
	    {
	      status =
		loadIcon(entry->icon->imageVariable, entry->icon->imageFile,
			 entry->icon->image);
	      if (status < 0)
		return (status);
	    }
	  memcpy(&(entry->iconParams.iconImage), entry->icon->image,
		 sizeof(image));
	}
      break;

    default:
      break;
    }

  return (status = 0);
}


static int changeDirectory(windowFileList *fileList, const char *rawPath)
{
  // Given a directory structure pointer, allocate memory, read all of the
  // required information into memory

  int status = 0;
  char path[MAX_PATH_LENGTH];
  char tmpFileName[MAX_PATH_NAME_LENGTH];
  int totalFiles = 0;
  fileEntry *tmpFileEntries = NULL;
  int tmpNumFileEntries = 0;
  file tmpFile;
  int count;

  fileFixupPath(rawPath, path);

  // Get the count of files so we can preallocate memory, etc.
  totalFiles = fileCount(path);
  if (totalFiles < 0)
    {
      error("Can't access directory \"%s\"", path);
      return (totalFiles);
    }

  // Read the file information for all the files
  if (totalFiles)
    {
      // Get memory for the new entries
      tmpFileEntries = malloc(totalFiles * sizeof(fileEntry));
      if (tmpFileEntries == NULL)
	{
	  error("Memory allocation error");
	  return (status = ERR_MEMORY);
	}
  
      for (count = 0; count < totalFiles; count ++)
	{
	  if (count == 0)
	    status = fileFirst(path, &tmpFile);
	  else
	    status = fileNext(path, &tmpFile);

	  if (status < 0)
	    {
	      error("Error reading files in \"%s\"", path);
	      free(tmpFileEntries);
	      return (status);
	    }
  
	  if (strcmp(tmpFile.name, "."))
	    {
	      memcpy(&(tmpFileEntries[tmpNumFileEntries].file), &tmpFile,
		     sizeof(file));
	      sprintf(tmpFileName, "%s/%s", path, tmpFile.name);
	      fileFixupPath(tmpFileName,
			    tmpFileEntries[tmpNumFileEntries].fullName);
	      if (!classifyEntry(&tmpFileEntries[tmpNumFileEntries]))
		tmpNumFileEntries += 1; 
	    }
	}
    }
  
  // Commit, baby.
  if (fileList->fileEntries)
    free(fileList->fileEntries);
  fileList->fileEntries = tmpFileEntries;
  fileList->numFileEntries = tmpNumFileEntries;

  return (status = 0);
}


static listItemParameters *allocateIconParameters(windowFileList *fileList)
{
  listItemParameters *newIconParams = NULL;
  int count;

  if (fileList->numFileEntries)
    {
      newIconParams =
	malloc(fileList->numFileEntries * sizeof(listItemParameters));
      if (newIconParams == NULL)
	{
	  error("Memory allocation error creating icon parameters");
	  return (newIconParams);
	}

      // Fill in an array of list item parameters structures for our file
      // entries.  It will get passed to the window list creation function
      // a moment
      for (count = 0; count < fileList->numFileEntries; count ++)
	memcpy(&(newIconParams[count]), (listItemParameters *)
	       &(((fileEntry *) fileList->fileEntries)[count].iconParams),
	       sizeof(listItemParameters));
    }

  return (newIconParams);
}


static int changeDirWithLock(windowFileList *fileList, const char *newDir)
{
  // Rescan the directory information and rebuild the file list, with locking
  // so that our GUI thread and main thread don't trash one another

  int status = 0;
  static lock dataLock;
  listItemParameters *iconParams = NULL;

  status = lockGet(&dataLock);
  if (status < 0)
    return (status);

  windowSwitchPointer(fileList->key, "busy");

  status = changeDirectory(fileList, newDir);
  if (status < 0)
    {
      windowSwitchPointer(fileList->key, "default");
      lockRelease(&dataLock);
      return (status);
    }

  iconParams = allocateIconParameters(fileList);
  if (iconParams == NULL)
    {
      windowSwitchPointer(fileList->key, "default");
      lockRelease(&dataLock);
      return (status = ERR_MEMORY);
    }

  windowComponentSetSelected(fileList->key, 0);
  windowComponentSetData(fileList->key, iconParams, fileList->numFileEntries);

  windowSwitchPointer(fileList->key, "default");

  free(iconParams);
  lockRelease(&dataLock);
  return (status = 0);
}


static int eventHandler(windowFileList *fileList, windowEvent *event)
{
  int status = 0;
  int clickedIcon = -1;
  fileEntry *fileEntries = (fileEntry *) fileList->fileEntries;
  listItemParameters *iconParams = NULL;

  // Get the selected item
  windowComponentGetSelected(fileList->key, &clickedIcon);
  if (clickedIcon < 0)
    return (status = clickedIcon);

  // Check for events in our icon list.  We consider the icon 'clicked'
  // if it is a mouse click selection, or an ENTER key selection
  if ((event->type & EVENT_SELECTION) &&
      ((event->type & EVENT_MOUSE_LEFTUP) ||
       ((event->type & EVENT_KEY_DOWN) && (event->key == 10))))
    {
      if (fileList->selectionCallback)
	fileList->selectionCallback((file *) &(fileEntries[clickedIcon].file),
				(char *) fileEntries[clickedIcon].fullName,
				(loaderFileClass *)
				&(fileEntries[clickedIcon].class));
     
      switch (fileEntries[clickedIcon].file.type)
	{
	case dirT:
	  if (fileList->browseFlags & WINFILEBROWSE_CAN_CD)
	    {
	      // Change to the directory, get the list of icon
	      // parameters, and update our window list.
	      status = changeDirWithLock(fileList,
					 fileEntries[clickedIcon].fullName);
	      if (status < 0)
		return (status);
	    }
	  break;
		  
	case linkT:
	  if ((fileList->browseFlags & WINFILEBROWSE_CAN_CD) &&
	      !strcmp((char *) fileEntries[clickedIcon].file
		      .name, ".."))
	    {
	      // Change to the directory, get the list of icon
	      // parameters, and update our window list.
	      status = changeDirWithLock(fileList, 
					 fileEntries[clickedIcon].fullName);
	      if (status < 0)
		return (status);
	    }
	  break;
		  
	default:
	  break;
	}
    }
  else if ((event->type & EVENT_KEY_DOWN) && (event->key == 127))
    {
      if (fileList->browseFlags & WINFILEBROWSE_CAN_DEL)
	{
	  windowSwitchPointer(fileList->key, "busy");

	  fileDeleteRecursive((char *) fileEntries[clickedIcon].fullName);

	  iconParams = allocateIconParameters(fileList);

	  windowSwitchPointer(fileList->key, "default");

	  if (iconParams)
	    {
	      windowComponentSetSelected(fileList, 0);
	      windowComponentSetData(fileList, iconParams,
				     fileList->numFileEntries);
	      free(iconParams);
	    }
	}
    }

  return (status = 0);
}


static int update(windowFileList *fileList, const char *directory)
{
  // Update the supplied file list from the supplied directory.  This is
  // useful for changing the current directory, for example.
  return (changeDirWithLock(fileList, directory));
}


static int destroy(windowFileList *fileList)
{
  // Detroy and deallocate the file list.

  if (fileList->fileEntries)
    free(fileList->fileEntries);

  free(fileList);
  return (0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ windowFileList *windowNewFileList(objectKey parent, windowListType type, int rows, int columns, const char *directory, int flags, void *callback, componentParameters *params)
{
  // Desc: Create a new file list widget with the parent window 'parent', the window list type 'type' (windowlist_textonly or windowlist_icononly is currently supported), of height 'rows' and width 'columns', the name of the starting location 'directory', flags (such as WINFILEBROWSE_CAN_CD or WINFILEBROWSE_CAN_DEL -- see sys/window.h), a function 'callback' for when the status changes, and component parameters 'params'.

  int status = 0;
  windowFileList *fileList = NULL;
  listItemParameters *iconParams = NULL;
  int count;

  // Check params.  Callback can be NULL.
  if ((parent == NULL) || (directory == NULL) || (params == NULL))
    {
      errno = ERR_NULLPARAMETER;
      return (fileList = NULL);
    }

  if (!initialized)
    {
      // Clear some memory
      bzero(&folderImage, sizeof(image));
      for (count = 0; count < (int) (sizeof(iconList) / sizeof(icon));
	   count ++)
	bzero(iconList[count].image, sizeof(image));
  
      // Try to read our config file
      status = configurationReader(FILEBROWSE_CONFIG, &config);
      if (status < 0)
	{
	  error("Can't locate configuration file %s", FILEBROWSE_CONFIG);
	  errno = ERR_NODATA;
	  return (fileList = NULL);
	}

      initialized = 1;
    }

  // Allocate memory for our file list
  fileList = malloc(sizeof(windowFileList));
  if (fileList == NULL)
    return (fileList);

  // Scan the directory
  status = changeDirectory(fileList, directory);
  if (status < 0)
    {
      fileList->destroy(fileList);
      errno = status;
      return (fileList = NULL);
    }

  // Get our array of icon parameters
  iconParams = allocateIconParameters(fileList);

  // Create a window list to hold the icons
  fileList->key = windowNewList(parent, type, rows, columns, 0, iconParams,
				fileList->numFileEntries, params);

  if (iconParams)
    free(iconParams);

  fileList->selectionCallback = callback;
  fileList->browseFlags = flags;

  fileList->eventHandler = &eventHandler;
  fileList->update = &update;
  fileList->destroy = &destroy;

  return (fileList);
}
