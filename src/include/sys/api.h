//
//	Visopsys
//	Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//	api.h
//

// This file describes all of the Visopsys kernel API functions that are
// exported to userspace.

#ifndef _API_H
#define _API_H

// This file should mostly never be included when we're compiling a kernel
// file
#if defined(KERNEL)
	#error "You cannot call the kernel API from within a kernel function"
#endif

#include <time.h>
#include <sys/apidefs.h>
#include <sys/color.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/file.h>
#include <sys/graphic.h>
#include <sys/guid.h>
#include <sys/image.h>
#include <sys/keyboard.h>
#include <sys/loader.h>
#include <sys/lock.h>
#include <sys/memory.h>
#include <sys/network.h>
#include <sys/process.h>
#include <sys/progress.h>
#include <sys/text.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/utsname.h>
#include <sys/vis.h>
#include <sys/window.h>

// Included in the Visopsys standard library to prevent API calls from
// within kernel code.
extern int visopsys_in_kernel;


//
// Text input/output functions
//
objectKey textGetConsoleInput(void);
int textSetConsoleInput(objectKey);
objectKey textGetConsoleOutput(void);
int textSetConsoleOutput(objectKey);
objectKey textGetCurrentInput(void);
int textSetCurrentInput(objectKey);
objectKey textGetCurrentOutput(void);
int textSetCurrentOutput(objectKey);
int textGetForeground(color *);
int textSetForeground(color *);
int textGetBackground(color *);
int textSetBackground(color *);
int textPutc(unsigned);
int textPutMbc(const char *);
int textPrint(const char *);
int textPrintAttrs(textAttrs *, const char *);
int textPrintLine(const char *);
void textNewline(void);
int textBackSpace(void);
int textTab(void);
int textCursorUp(void);
int textCursorDown(void);
int textCursorLeft(void);
int textCursorRight(void);
int textEnableScroll(int);
void textScroll(int);
int textGetNumColumns(void);
int textGetNumRows(void);
int textGetColumn(void);
void textSetColumn(int);
int textGetRow(void);
void textSetRow(int);
void textSetCursor(int);
int textScreenClear(void);
int textScreenSave(textScreen *);
int textScreenRestore(textScreen *);
int textInputStreamCount(objectKey);
int textInputCount(void);
int textInputStreamGetc(objectKey, unsigned *);
int textInputGetc(unsigned *);
int textInputStreamReadN(objectKey, int, unsigned *);
int textInputReadN(int, unsigned *);
int textInputStreamReadAll(objectKey, unsigned *);
int textInputReadAll(unsigned *);
int textInputStreamAppend(objectKey, unsigned);
int textInputAppend(unsigned);
int textInputStreamAppendN(objectKey, int, unsigned *);
int textInputAppendN(int, unsigned *);
int textInputStreamRemoveAll(objectKey);
int textInputRemoveAll(void);
void textInputStreamSetEcho(objectKey, int);
void textInputSetEcho(int);

//
// Disk functions
//
int diskReadPartitions(const char *);
int diskReadPartitionsAll(void);
int diskSync(const char *);
int diskSyncAll(void);
int diskGetBoot(char *);
int diskGetCount(void);
int diskGetPhysicalCount(void);
int diskGet(const char *, disk *);
int diskGetAll(disk *, unsigned);
int diskGetAllPhysical(disk *, unsigned);
int diskGetFilesystemType(const char *, char *, unsigned);
int diskGetMsdosPartType(int, msdosPartType *);
msdosPartType *diskGetMsdosPartTypes(void);
int diskGetGptPartType(guid *, gptPartType *);
gptPartType *diskGetGptPartTypes(void);
int diskSetFlags(const char *, unsigned, int);
int diskSetLockState(const char *, int);
int diskSetDoorState(const char *, int);
int diskMediaPresent(const char *);
int diskReadSectors(const char *, uquad_t, uquad_t, void *);
int diskWriteSectors(const char *, uquad_t, uquad_t, const void *);
int diskEraseSectors(const char *, uquad_t, uquad_t, int);
int diskGetStats(const char *, diskStats *);
int diskRamDiskCreate(unsigned, char *);
int diskRamDiskDestroy(const char *);

//
// Filesystem functions
//
int filesystemScan(const char *);
int filesystemFormat(const char *, const char *, const char *, int,
	progress *);
int filesystemClobber(const char *);
int filesystemCheck(const char *, int, int, progress *);
int filesystemDefragment(const char *, progress *);
int filesystemResizeConstraints(const char *, uquad_t *, uquad_t *,
	progress *);
int filesystemResize(const char *, uquad_t, progress *);
int filesystemMount(const char *, const char *);
int filesystemUnmount(const char *);
uquad_t filesystemGetFreeBytes(const char *);
unsigned filesystemGetBlockSize(const char *);

//
// File functions
//
int fileFixupPath(const char *, char *);
int fileGetDisk(const char *, disk *);
int fileCount(const char *);
int fileFirst(const char *, file *);
int fileNext(const char *, file *);
int fileFind(const char *, file *);
int fileOpen(const char *, int, file *);
int fileClose(file *);
int fileRead(file *, unsigned, unsigned, void *);
int fileWrite(file *, unsigned, unsigned, void *);
int fileDelete(const char *);
int fileDeleteRecursive(const char *);
int fileDeleteSecure(const char *, int);
int fileMakeDir(const char *);
int fileRemoveDir(const char *);
int fileCopy(const char *, const char *);
int fileCopyRecursive(const char *, const char *);
int fileMove(const char *, const char *);
int fileTimestamp(const char *);
int fileSetSize(file *, unsigned);
int fileGetTempName(char *, unsigned);
int fileGetTemp(file *);
int fileGetFullPath(file *, char *, int);
int fileStreamOpen(const char *, int, fileStream *);
int fileStreamSeek(fileStream *, unsigned);
int fileStreamRead(fileStream *, unsigned, char *);
int fileStreamReadLine(fileStream *, unsigned, char *);
int fileStreamWrite(fileStream *, unsigned, const char *);
int fileStreamWriteStr(fileStream *, const char *);
int fileStreamWriteLine(fileStream *, const char *);
int fileStreamFlush(fileStream *);
int fileStreamClose(fileStream *);
int fileStreamGetTemp(fileStream *);

//
// Memory functions
//
void *memoryGet(unsigned, const char *);
int memoryRelease(void *);
int memoryReleaseAllByProcId(int);
int memoryGetStats(memoryStats *, int);
int memoryGetBlocks(memoryBlock *, unsigned, int);

//
// Multitasker functions
//
int multitaskerCreateProcess(const char *, int, processImage *);
int multitaskerSpawn(void *, const char *, int, void *[], int);
int multitaskerGetCurrentProcessId(void);
int multitaskerGetProcess(int, process *);
int multitaskerGetProcessByName(const char *, process *);
int multitaskerGetProcesses(void *, unsigned);
int multitaskerSetProcessState(int, int);
int multitaskerProcessIsAlive(int);
int multitaskerSetProcessPriority(int, int);
int multitaskerGetProcessPrivilege(int);
int multitaskerGetCurrentDirectory(char *, int);
int multitaskerSetCurrentDirectory(const char *);
objectKey multitaskerGetTextInput(void);
int multitaskerSetTextInput(int, objectKey);
objectKey multitaskerGetTextOutput(void);
int multitaskerSetTextOutput(int, objectKey);
int multitaskerDuplicateIo(int, int, int);
int multitaskerGetProcessorTime(clock_t *);
void multitaskerYield(void);
void multitaskerWait(unsigned);
int multitaskerBlock(int);
int multitaskerDetach(void);
int multitaskerKillProcess(int);
int multitaskerKillByName(const char *);
int multitaskerTerminate(int);
int multitaskerSignalSet(int, int, int);
int multitaskerSignal(int, int);
int multitaskerSignalRead(int);
int multitaskerGetIoPerm(int, int);
int multitaskerSetIoPerm(int, int, int);
int multitaskerStackTrace(int);

//
// Loader functions
//
void *loaderLoad(const char *, file *);
objectKey loaderClassify(const char *, void *, unsigned, loaderFileClass *);
objectKey loaderClassifyFile(const char *, loaderFileClass *);
loaderSymbolTable *loaderGetSymbols(const char *);
int loaderCheckCommand(const char *);
int loaderLoadProgram(const char *, int);
int loaderLoadLibrary(const char *);
void *loaderGetLibrary(const char *);
void *loaderLinkLibrary(const char *);
void *loaderGetSymbol(const char *);
int loaderExecProgram(int, int);
int loaderLoadAndExec(const char *, int, int);

//
// Real-time clock functions
//
int rtcReadSeconds(void);
int rtcReadMinutes(void);
int rtcReadHours(void);
int rtcDayOfWeek(unsigned, unsigned, unsigned);
int rtcReadDayOfMonth(void);
int rtcReadMonth(void);
int rtcReadYear(void);
unsigned rtcUptimeSeconds(void);
int rtcDateTime(struct tm *);

//
// Random number functions
//
unsigned randomUnformatted(void);
unsigned randomFormatted(unsigned, unsigned);
unsigned randomSeededUnformatted(unsigned);
unsigned randomSeededFormatted(unsigned, unsigned, unsigned);
void randomBytes(unsigned char *, unsigned);

//
// Environment functions
//
int environmentGet(const char *, char *, unsigned);
int environmentSet(const char *, const char *);
int environmentUnset(const char *);
void environmentDump(void);

//
// Raw graphics functions
//
int graphicsAreEnabled(void);
int graphicGetModes(videoMode *, unsigned);
int graphicGetMode(videoMode *);
int graphicSetMode(videoMode *);
int graphicGetScreenWidth(void);
int graphicGetScreenHeight(void);
int graphicCalculateAreaBytes(int, int);
int graphicClearScreen(color *);
int graphicDrawPixel(graphicBuffer *, color *, drawMode, int, int);
int graphicDrawLine(graphicBuffer *, color *, drawMode, int, int, int, int);
int graphicDrawRect(graphicBuffer *, color *, drawMode, int, int, int, int,
	int, int);
int graphicDrawOval(graphicBuffer *, color *, drawMode, int, int, int, int,
	int, int);
int graphicGetImage(graphicBuffer *, image *, int, int, int, int);
int graphicDrawImage(graphicBuffer *, image *, drawMode, int, int, int, int,
	int, int);
int graphicDrawText(graphicBuffer *, color *, color *, objectKey,
	const char *, const char *, drawMode, int, int);
int graphicCopyArea(graphicBuffer *, int, int, int, int, int, int);
int graphicClearArea(graphicBuffer *, color *, int, int, int, int);
int graphicRenderBuffer(graphicBuffer *, int, int, int, int, int, int);

//
// Image functions
//
int imageNew(image *, unsigned, unsigned);
int imageFree(image *);
int imageLoad(const char *, unsigned, unsigned, image *);
int imageSave(const char *, int, image *);
int imageResize(image *, unsigned, unsigned);
int imageCopy(image *, image *);
int imageFill(image *, color *);
int imagePaste(image *, image *, int, int);

//
// Font functions
//
objectKey fontGetSystem(void);
objectKey fontGet(const char *, unsigned, int, const char *);
int fontGetPrintedWidth(objectKey, const char *, const char *);
int fontGetWidth(objectKey);
int fontGetHeight(objectKey);

//
// Windowing system functions
//
int windowLogin(const char *, const char *);
int windowLogout(const char *);
objectKey windowNew(int, const char *);
objectKey windowNewDialog(objectKey, const char *);
int windowDestroy(objectKey);
int windowGetList(objectKey *, int);
int windowGetInfo(objectKey, windowInfo *);
int windowSetCharSet(objectKey, const char *);
int windowSetTitle(objectKey, const char *);
int windowSetSize(objectKey, int, int);
int windowSetLocation(objectKey, int, int);
int windowCenter(objectKey);
int windowSnapIcons(objectKey);
int windowSetHasBorder(objectKey, int);
int windowSetHasTitleBar(objectKey, int);
int windowSetMovable(objectKey, int);
int windowSetResizable(objectKey, int);
int windowSetFocusable(objectKey, int);
int windowSetRoot(objectKey);
int windowRemoveMinimizeButton(objectKey);
int windowRemoveCloseButton(objectKey);
int windowSetVisible(objectKey, int);
void windowSetMinimized(objectKey, int);
int windowAddConsoleTextArea(objectKey);
int windowGetColor(const char *, color *);
int windowSetColor(const char *, color *);
void windowResetColors(void);
int windowComponentEventGet(objectKey, windowEvent *);
int windowSetBackgroundColor(objectKey, color *);
int windowSetBackgroundImage(objectKey, image *);
int windowShellChangeBackground(void);
objectKey windowShellNewTaskbarIcon(image *);
objectKey windowShellNewTaskbarTextLabel(const char *);
int windowShellDestroyTaskbarComp(objectKey);
objectKey windowShellIconify(objectKey, int, image *);
int windowScreenShot(image *);
int windowSaveScreenShot(const char *);
int windowSetTextOutput(objectKey);
int windowLayout(objectKey);
void windowDebugLayout(objectKey);
int windowContextSet(objectKey, objectKey);
int windowSwitchPointer(objectKey, const char *);
int windowToggleMenuBar(objectKey);
int windowRefresh(void);
void windowComponentDestroy(objectKey);
int windowComponentSetCharSet(objectKey, const char *);
int windowComponentSetVisible(objectKey, int);
int windowComponentSetEnabled(objectKey, int);
int windowComponentGetWidth(objectKey);
int windowComponentSetWidth(objectKey, int);
int windowComponentGetHeight(objectKey);
int windowComponentSetHeight(objectKey, int);
int windowComponentFocus(objectKey);
int windowComponentUnfocus(objectKey);
int windowComponentLayout(objectKey);
int windowComponentDraw(objectKey);
int windowComponentGetData(objectKey, void *, int);
int windowComponentSetData(objectKey, void *, int, int);
int windowComponentAppendData(objectKey, void *, int, int);
int windowComponentGetSelected(objectKey, int *);
int windowComponentSetSelected(objectKey, int );
objectKey windowNewButton(objectKey, const char *, image *,
	componentParameters *);
objectKey windowNewCanvas(objectKey, int, int, componentParameters *);
objectKey windowNewCheckbox(objectKey, const char *, componentParameters *);
objectKey windowNewContainer(objectKey, const char *, componentParameters *);
objectKey windowNewDivider(objectKey, dividerType, componentParameters *);
objectKey windowNewIcon(objectKey, image *, const char *,
	componentParameters *);
objectKey windowNewImage(objectKey, image *, drawMode, componentParameters *);
objectKey windowNewList(objectKey, windowListType, int, int, int,
	listItemParameters *, int, componentParameters *);
objectKey windowNewListItem(objectKey, windowListType, listItemParameters *,
	componentParameters *);
objectKey windowNewMenu(objectKey, objectKey,
	const char *, windowMenuContents *, componentParameters *);
objectKey windowNewMenuBar(objectKey, componentParameters *);
objectKey windowNewMenuBarIcon(objectKey, image *, componentParameters *);
objectKey windowNewMenuItem(objectKey, const char *, componentParameters *);
objectKey windowNewPasswordField(objectKey, int, componentParameters *);
objectKey windowNewProgressBar(objectKey, componentParameters *);
objectKey windowNewRadioButton(objectKey, int, int, char *[], int,
	componentParameters *);
objectKey windowNewScrollBar(objectKey, scrollBarType, int, int,
	componentParameters *);
objectKey windowNewSlider(objectKey, scrollBarType, int, int,
	componentParameters *);
objectKey windowNewTextArea(objectKey, int, int, int, componentParameters *);
objectKey windowNewTextField(objectKey, int, componentParameters *);
objectKey windowNewTextLabel(objectKey, const char *, componentParameters *);
objectKey windowNewTree(objectKey, windowTreeItem *, int, int,
	componentParameters *);
int windowMenuUpdate(objectKey, const char *, const char *,
	windowMenuContents *, componentParameters *);
int windowMenuDestroy(objectKey);

//
// User functions
//
int userAuthenticate(const char *, const char *);
int userLogin(const char *, const char *, int);
int userLogout(const char *);
int userExists(const char *);
int userGetNames(char *, unsigned);
int userAdd(const char *, const char *);
int userDelete(const char *);
int userSetPassword(const char *, const char *, const char *);
int userGetCurrent(char *, unsigned);
int userGetPrivilege(const char *);
int userGetSessions(userSession *, int);
int userFileAdd(const char *, const char *, const char *);
int userFileDelete(const char *, const char *);
int userFileSetPassword(const char *, const char *, const char *, const char *);

//
// Network functions
//
int networkEnabled(void);
int networkEnable(void);
int networkDisable(void);
objectKey networkOpen(int, networkAddress *, networkFilter *);
int networkClose(objectKey);
int networkConnectionGetCount(void);
int networkConnectionGetAll(networkConnection *, unsigned);
int networkCount(objectKey);
int networkRead(objectKey, unsigned char *, unsigned);
int networkWrite(objectKey, unsigned char *, unsigned);
int networkPing(objectKey, int, unsigned char *, unsigned);
int networkGetHostName(char *, int);
int networkSetHostName(const char *, int);
int networkGetDomainName(char *, int);
int networkSetDomainName(const char *, int);
int networkLookupNameAddress(const char *, networkAddress *, int *);
int networkLookupAddressName(const networkAddress *, char *, unsigned);
int networkDeviceEnable(const char *);
int networkDeviceDisable(const char *);
int networkDeviceGetCount(void);
int networkDeviceGet(const char *, networkDevice *);
int networkDeviceHook(const char *, objectKey *, int);
int networkDeviceUnhook(const char *, objectKey, int);
unsigned networkDeviceSniff(objectKey, unsigned char *, unsigned);

//
// Inter-process communication functions
//
objectKey pipeNew(unsigned, unsigned);
int pipeDestroy(objectKey);
int pipeSetReader(objectKey, int);
int pipeSetWriter(objectKey, int);
int pipeClear(objectKey);
int pipeRead(objectKey, unsigned, void *);
int pipeWrite(objectKey, unsigned, void *);

//
// Miscellaneous functions
//
int systemShutdown(int, int);
void getVersion(char *, int);
int systemInfo(struct utsname *);
int cryptHashMd5(const unsigned char *, unsigned, unsigned char *);
int cryptHashSha1(const unsigned char *, unsigned, unsigned char *, int,
	unsigned);
int cryptHashSha1Cont(const unsigned char *, unsigned, unsigned char *, int,
	unsigned);
int cryptHashSha256(const unsigned char *, unsigned, unsigned char *, int,
	unsigned);
int cryptHashSha256Cont(const unsigned char *, unsigned, unsigned char *, int,
	unsigned);
int lockGet(spinLock *);
int lockRelease(spinLock *);
int lockVerify(spinLock *);
int configRead(const char *, variableList *);
int configWrite(const char *, variableList *);
int configGet(const char *, const char *, char *, unsigned);
int configSet(const char *, const char *, const char *);
int configUnset(const char *, const char *);
int guidGenerate(guid *);
unsigned crc32(void *, unsigned, unsigned *);
int keyboardGetMap(keyMap *);
int keyboardSetMap(const char *);
int keyboardVirtualInput(int, keyScan);
int deviceTreeGetRoot(device *);
int deviceTreeGetChild(device *, device *);
int deviceTreeGetNext(device *);
int mouseLoadPointer(const char *, const char *);
void *pageGetPhysical(int, void *);
unsigned charsetToUnicode(const char *, unsigned);
unsigned charsetFromUnicode(const char *, unsigned);
uquad_t cpuGetMs(void);
void cpuSpinMs(unsigned);
int touchAvailable(void);

#endif

