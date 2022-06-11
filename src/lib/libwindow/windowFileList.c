// 
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/ascii.h>
#include <sys/window.h>

#define _(string) gettext(string)
#define FILEBROWSE_CONFIG        "/system/config/filebrowse.conf"
#define DEFAULT_FOLDERICON_VAR   "icon.folder"
#define DEFAULT_FOLDERICON_FILE  "/system/icons/foldicon.bmp"
#define DEFAULT_FILEICON_VAR     "icon.file"
#define DEFAULT_FILEICON_FILE    "/system/icons/pageicon.bmp"
#define DEFAULT_IMAGEICON_VAR    "icon.image"
#define DEFAULT_IMAGEICON_FILE   "/system/icons/imageicon.bmp"
#define DEFAULT_EXECICON_VAR     "icon.executable"
#define DEFAULT_EXECICON_FILE    "/system/icons/execicon.bmp"
#define DEFAULT_MESSAGEICON_VAR  "icon.message"
#define DEFAULT_MESSAGEICON_FILE "/system/icons/mesgicon.ico"
#define DEFAULT_OBJICON_VAR      "icon.object"
#define DEFAULT_OBJICON_FILE     "/system/icons/objicon.bmp"
#define DEFAULT_BOOTICON_VAR     "icon.boot"
#define DEFAULT_BOOTICON_FILE    "/system/icons/booticon.bmp"
#define DEFAULT_KEYMAPICON_VAR   "icon.keymap"
#define DEFAULT_KEYMAPICON_FILE  "/system/icons/kmapicon.ico"
#define DEFAULT_PDFICON_VAR      "icon.pdf"
#define DEFAULT_PDFICON_FILE     "/system/icons/pdficon.ico"
#define DEFAULT_ARCHICON_VAR     "icon.archive"
#define DEFAULT_ARCHICON_FILE    "/system/icons/archicon.ico"
#define DEFAULT_FONTICON_VAR     "icon.font"
#define DEFAULT_FONTICON_FILE    "/system/icons/fonticon.ico"
#define DEFAULT_CONFIGICON_VAR   "icon.config"
#define DEFAULT_CONFIGICON_FILE  "/system/icons/conficon.bmp"
#define DEFAULT_HTMLICON_VAR     "icon.html"
#define DEFAULT_HTMLICON_FILE    "/system/icons/htmlicon.bmp"
#define DEFAULT_TEXTICON_VAR     "icon.text"
#define DEFAULT_TEXTICON_FILE    "/system/icons/texticon.bmp"
#define DEFAULT_BINICON_VAR      "icon.binary"
#define DEFAULT_BINICON_FILE     "/system/icons/binicon.bmp"

typedef struct {
  int class;
  int subClass;
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

extern int libwindow_initialized;
extern void libwindowInitialize(void);

static variableList config;

// Our list of icon images
static image folderImage;
static image fileImage;
static image imageImage;
static image bootImage;
static image keymapImage;
static image pdfImage;
static image archImage;
static image fontImage;
static image execImage;
static image messageImage;
static image objImage;
static image configImage;
static image htmlImage;
static image textImage;
static image binImage;

#define FOLDER_ICON \
  { LOADERFILECLASS_NONE, LOADERFILESUBCLASS_NONE, DEFAULT_FOLDERICON_VAR, \
    DEFAULT_FOLDERICON_FILE, &folderImage }
#define TEXT_ICON \
  { LOADERFILECLASS_TEXT, LOADERFILESUBCLASS_NONE, DEFAULT_TEXTICON_VAR, \
    DEFAULT_TEXTICON_FILE, &textImage }
#define BIN_ICON \
  { LOADERFILECLASS_BIN, LOADERFILESUBCLASS_NONE, DEFAULT_BINICON_VAR, \
    DEFAULT_BINICON_FILE, &binImage }
#define FILE_ICON \
 { 0xFFFFFFFF, 0xFFFFFFFF, DEFAULT_FILEICON_VAR, DEFAULT_FILEICON_FILE, \
   &fileImage }

static icon folderIcon = FOLDER_ICON;
static icon textIcon = TEXT_ICON;
static icon binIcon = BIN_ICON;
static icon fileIcon = FILE_ICON;

static icon iconList[] = {
  // These get traversed in order; the first matching file class flags get
  // the icon.  So, for example, if you want to make an icon for a type
  // of binary file, put it *before* the icon for plain binaries.
  { LOADERFILECLASS_IMAGE, LOADERFILESUBCLASS_NONE, DEFAULT_IMAGEICON_VAR,
    DEFAULT_IMAGEICON_FILE, &imageImage },
  { LOADERFILECLASS_BOOT, LOADERFILESUBCLASS_NONE, DEFAULT_BOOTICON_VAR,
    DEFAULT_BOOTICON_FILE, &bootImage },
  { LOADERFILECLASS_KEYMAP, LOADERFILESUBCLASS_NONE, DEFAULT_KEYMAPICON_VAR,
    DEFAULT_KEYMAPICON_FILE, &keymapImage },
  { LOADERFILECLASS_DOC, LOADERFILESUBCLASS_PDF, DEFAULT_PDFICON_VAR,
    DEFAULT_PDFICON_FILE, &pdfImage },
  { LOADERFILECLASS_ARCHIVE, LOADERFILESUBCLASS_NONE, DEFAULT_ARCHICON_VAR,
    DEFAULT_ARCHICON_FILE, &archImage },
  { LOADERFILECLASS_FONT, LOADERFILESUBCLASS_NONE, DEFAULT_FONTICON_VAR,
    DEFAULT_FONTICON_FILE, &fontImage },
  { LOADERFILECLASS_EXEC, LOADERFILESUBCLASS_NONE, DEFAULT_EXECICON_VAR,
    DEFAULT_EXECICON_FILE, &execImage },
  { LOADERFILECLASS_OBJ, LOADERFILESUBCLASS_MESSAGE, DEFAULT_MESSAGEICON_VAR,
    DEFAULT_MESSAGEICON_FILE, &messageImage },
  { (LOADERFILECLASS_OBJ | LOADERFILECLASS_LIB), LOADERFILESUBCLASS_NONE,
    DEFAULT_OBJICON_VAR, DEFAULT_OBJICON_FILE, &objImage },
  { LOADERFILECLASS_DATA, LOADERFILESUBCLASS_CONFIG, DEFAULT_CONFIGICON_VAR,
    DEFAULT_CONFIGICON_FILE, &configImage },
  { LOADERFILECLASS_DOC, LOADERFILESUBCLASS_HTML, DEFAULT_HTMLICON_VAR,
    DEFAULT_HTMLICON_FILE, &htmlImage },
  TEXT_ICON,
  BIN_ICON,
  // This one goes last, because the flags match every file class.
  FILE_ICON,
  { NULL, NULL, NULL, NULL, NULL }
};
    

__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
  // Generic error message code
  
  va_list list;
  char *output = NULL;

  output = malloc(MAXSTRINGLENGTH);
  if (output == NULL)
    return;
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  windowNewErrorDialog(NULL, _("Error"), output);
  free(output);
}


static int loadIcon(const char *variableName, const char *defaultIcon,
		    image *theImage)
{
  // Try to load the requested icon, first based on the configuration file
  // variable name, then by the default filename.

  int status = 0;
  char variableValue[MAX_PATH_NAME_LENGTH];

  // First try the variable
  status = variableListGet(&config, variableName, variableValue,
			   MAX_PATH_NAME_LENGTH);
  if (status >= 0)
    defaultIcon = variableValue;

  // Try to load the image
  status = fileFind(defaultIcon, NULL);
  if (status < 0)
    return (status);

  return (imageLoad(defaultIcon, 0, 0, theImage));
}


static void getFileIcon(fileEntry *entry)
{
  // Choose the appropriate icon for the class of file, and load it if
  // necessary.

  int count;

  entry->icon = &fileIcon;

  // Try to find an exact match.  If there isn't one, this should default
  // to the 'file' type at the end of the list.
  for (count = 0; iconList[count].image ; count ++)
    {
      if (((iconList[count].class == LOADERFILECLASS_NONE) ||
	   (entry->class.class & iconList[count].class)) &&
	  ((iconList[count].subClass == LOADERFILESUBCLASS_NONE) ||
	   (entry->class.subClass & iconList[count].subClass)))
	{
	  entry->icon = &iconList[count];
	  break;
	}
    }

  // Do we need to load the image data?
  while (entry->icon->image->data == NULL)
    {
      if (loadIcon(entry->icon->imageVariable, entry->icon->imageFile,
		   entry->icon->image) < 0)
	{
	  // Even the 'file' icon image failed.
	  if (entry->icon == &fileIcon)
	    return;

	  // If it's a binary file, try the binary icon image
	  if ((entry->icon != &binIcon) &&
	      (entry->class.class & LOADERFILECLASS_BIN))
	    entry->icon = &binIcon;

	  else if ((entry->icon != &textIcon) &&
		   (entry->class.class & LOADERFILECLASS_TEXT))
	    entry->icon = &textIcon;

	  else
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
	strcpy(entry->iconParams.text, _("(up)"));
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
	  strcpy(entry->iconParams.text, _("(up)"));
	  entry->icon = &folderIcon;
	  if (entry->icon->image->data == NULL)
	    {
	      status = loadIcon(entry->icon->imageVariable,
				entry->icon->imageFile, entry->icon->image);
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
      error(_("Can't get directory \"%s\" file count"), path);
      return (totalFiles);
    }

  // Read the file information for all the files
  if (totalFiles)
    {
      // Get memory for the new entries
      tmpFileEntries = malloc(totalFiles * sizeof(fileEntry));
      if (tmpFileEntries == NULL)
	{
	  error("%s", _("Memory allocation error"));
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
	      error(_("Error reading files in \"%s\""), path);
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
  strncpy(fileList->cwd, path, MAX_PATH_LENGTH);
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
	  error("%s", _("Memory allocation error creating icon parameters"));
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

  // Clear the list
  windowComponentSetData(fileList->key, NULL, 0);
  windowComponentSetData(fileList->key, iconParams, fileList->numFileEntries);
  windowComponentSetSelected(fileList->key, 0);

  windowSwitchPointer(fileList->key, "default");

  free(iconParams);
  lockRelease(&dataLock);
  return (status = 0);
}


static int update(windowFileList *fileList)
{
  // Update the supplied file list from the supplied directory.  This is
  // useful for changing the current directory, for example.
  return (changeDirWithLock(fileList, fileList->cwd));
}


static int destroy(windowFileList *fileList)
{
  // Detroy and deallocate the file list.

  int count;

  if (fileList->fileEntries)
    free(fileList->fileEntries);

  free(fileList);

  if (folderImage.data)
    imageFree(&folderImage);

  for (count = 0; iconList[count].image ; count ++)
    if (iconList[count].image->data)
      imageFree(iconList[count].image);

  variableListDestroy(&config);

  return (0);
}


static int eventHandler(windowFileList *fileList, windowEvent *event)
{
  int status = 0;
  int selected = -1;
  fileEntry *fileEntries = (fileEntry *) fileList->fileEntries;
  fileEntry saveEntry;

  // Get the selected item
  windowComponentGetSelected(fileList->key, &selected);
  if (selected < 0)
    return (status = selected);

  // Check for events in our icon list.  We consider the icon 'clicked'
  // if it is a mouse click selection, or an ENTER key selection
  if ((event->type & EVENT_SELECTION) &&
      ((event->type & EVENT_MOUSE_LEFTUP) ||
       ((event->type & EVENT_KEY_DOWN) && (event->key == ASCII_ENTER))))
    {
      memcpy(&saveEntry, &fileEntries[selected], sizeof(fileEntry));

      if ((fileEntries[selected].file.type == linkT) &&
	  !strcmp((char *) fileEntries[selected].file.name, ".."))
	saveEntry.file.type = dirT;

      if ((saveEntry.file.type == dirT) &&
	  (fileList->browseFlags & WINFILEBROWSE_CAN_CD))
	{
	  // Change to the directory, get the list of icon
	  // parameters, and update our window list.
	  status =
	    changeDirWithLock(fileList, saveEntry.fullName);
	  if (status < 0)
	    {
	      error(_("Can't change to directory %s"), saveEntry.file.name);
	      return (status);
	    }
	}

      if (fileList->selectionCallback)
	fileList->selectionCallback((file *) &saveEntry.file,
				    (char *) saveEntry.fullName,
				    (loaderFileClass *) &saveEntry.class);
    }

  else if ((event->type & EVENT_KEY_DOWN) && (event->key == ASCII_DEL))
    {
      if ((fileList->browseFlags & WINFILEBROWSE_CAN_DEL) &&
	  strcmp((char *) fileEntries[selected].file.name, ".."))
	{
	  windowSwitchPointer(fileList->key, "busy");

	  status =
	    fileDeleteRecursive((char *) fileEntries[selected].fullName);

	  windowSwitchPointer(fileList->key, "default");

	  if (status < 0)
	    error(_("Error deleting file %s"),
		  fileEntries[selected].file.name);

	  status = update(fileList);
	  if (status < 0)
	    return (status);

	  if (selected >= fileList->numFileEntries)
	    selected = (fileList->numFileEntries - 1);

	  windowComponentSetSelected(fileList, selected);
	}
    }

  return (status = 0);
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

  if (!libwindow_initialized)
    libwindowInitialize();

  // Check params.  Callback can be NULL.
  if ((parent == NULL) || (directory == NULL) || (params == NULL))
    {
      errno = ERR_NULLPARAMETER;
      return (fileList = NULL);
    }

  // Initialize the images
  bzero(&folderImage, sizeof(image));
  for (count = 0; iconList[count].image ; count ++)
    bzero(iconList[count].image, sizeof(image));

  // Try to read our config file
  status = configRead(FILEBROWSE_CONFIG, &config);
  if (status < 0)
    {
      error(_("Can't locate configuration file %s"), FILEBROWSE_CONFIG);
      errno = ERR_NODATA;
      return (fileList = NULL);
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
